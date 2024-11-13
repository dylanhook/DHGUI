// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "render/image_decoder.h"

#include <Windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace dh::render::image {

    namespace {

        using Microsoft::WRL::ComPtr;

        class com_apartment final {
        public:
            com_apartment() noexcept {
                const HRESULT result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);

                m_result = result;
                m_uninitialize = result == S_OK || result == S_FALSE;
            }
            ~com_apartment() {
                if (m_uninitialize) ::CoUninitialize();
            }

            com_apartment(const com_apartment&) = delete;
            com_apartment& operator=(const com_apartment&) = delete;
            com_apartment(com_apartment&&) = delete;
            com_apartment& operator=(com_apartment&&) = delete;

            [[nodiscard]] bool available() const noexcept { return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE; }
            [[nodiscard]] HRESULT result() const noexcept { return m_result; }

        private:
            HRESULT m_result{E_UNEXPECTED};
            bool m_uninitialize{false};
        };

        [[nodiscard]] dh::unexpected<decode_failure> fail(decode_error cause, HRESULT result = S_OK) noexcept {
            return dh::unexpected<decode_failure>{decode_failure{cause, static_cast<long>(result)}};
        }

        [[nodiscard]] dh::u32 read_big_endian_u32(const dh::u8* bytes) noexcept {
            return (static_cast<dh::u32>(bytes[0]) << 24) | (static_cast<dh::u32>(bytes[1]) << 16) |
                (static_cast<dh::u32>(bytes[2]) << 8) | static_cast<dh::u32>(bytes[3]);
        }

        constexpr dh::u8 k_png_signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        constexpr dh::u8 k_jpeg_signature[] = {0xFF, 0xD8, 0xFF};
        constexpr dh::u8 k_bmp_signature[] = {'B', 'M'};

        [[nodiscard]] bool starts_with(dh::span<const dh::u8> source, dh::span<const dh::u8> signature) noexcept {
            if (source.size() < signature.size()) return false;
            for (dh::size_t index = 0; index < signature.size(); ++index) {
                if (source[index] != signature[index]) return false;
            }
            return true;
        }

        [[nodiscard]] bool declares_animation(dh::span<const dh::u8> source) noexcept {
            constexpr dh::span<const dh::u8> k_signature{k_png_signature};
            constexpr dh::size_t k_chunk_header_bytes = 8;
            constexpr dh::size_t k_chunk_crc_bytes = 4;
            if (!starts_with(source, k_signature)) return false;

            dh::size_t at = k_signature.size();
            while (source.size() - at >= k_chunk_header_bytes + k_chunk_crc_bytes) {
                const dh::size_t payload = read_big_endian_u32(source.data() + at);
                if (payload > source.size() - at - k_chunk_header_bytes - k_chunk_crc_bytes) return false;
                const dh::u8* const type = source.data() + at + 4;
                if (type[0] == 'a' && type[1] == 'c' && type[2] == 'T' && type[3] == 'L') return true;
                at += k_chunk_header_bytes + payload + k_chunk_crc_bytes;
            }
            return false;
        }

    }

    dh::string to_string(decode_error error) noexcept {
        switch (error) {
            case decode_error::source_empty:
                return dh::string{"source_empty"};
            case decode_error::source_too_large:
                return dh::string{"source_too_large"};
            case decode_error::format_invalid: return dh::string{"format_invalid"};
            case decode_error::com_initialize_failed:
                return dh::string{"com_initialize_failed"};
            case decode_error::factory_create_failed:
                return dh::string{"factory_create_failed"};
            case decode_error::stream_create_failed:
                return dh::string{"stream_create_failed"};
            case decode_error::stream_initialize_failed:
                return dh::string{"stream_initialize_failed"};
            case decode_error::decoder_create_failed:
                return dh::string{"decoder_create_failed"};
            case decode_error::decoder_initialize_failed:
                return dh::string{"decoder_initialize_failed"};
            case decode_error::format_mismatch:
                return dh::string{"format_mismatch"};
            case decode_error::frame_count_failed:
                return dh::string{"frame_count_failed"};
            case decode_error::animation_unsupported:
                return dh::string{"animation_unsupported"};
            case decode_error::frame_get_failed:
                return dh::string{"frame_get_failed"};
            case decode_error::size_query_failed:
                return dh::string{"size_query_failed"};
            case decode_error::dimensions_out_of_range:
                return dh::string{"dimensions_out_of_range"};
            case decode_error::output_limit_exceeded:
                return dh::string{"output_limit_exceeded"};
            case decode_error::converter_create_failed:
                return dh::string{"converter_create_failed"};
            case decode_error::converter_initialize_failed:
                return dh::string{"converter_initialize_failed"};
            case decode_error::pixel_copy_failed:
                return dh::string{"pixel_copy_failed"};
        }
        return dh::string{"unknown"};
    }

    dh::expected<decoded_image, decode_failure> decode(dh::span<const dh::u8> source, format source_format, const dh::size_t output_limit_bytes) noexcept {
        if (source.empty()) return fail(decode_error::source_empty);
        if (source.size() > k_max_source_bytes) return fail(decode_error::source_too_large);
        if (source_format != format::png && source_format != format::jpeg && source_format != format::bmp)
            return fail(decode_error::format_invalid);

        const dh::span<const dh::u8> signature = source_format == format::png ? dh::span<const dh::u8>{k_png_signature} :
            source_format == format::jpeg ? dh::span<const dh::u8>{k_jpeg_signature} : dh::span<const dh::u8>{k_bmp_signature};
        if (!starts_with(source, signature)) return fail(decode_error::format_mismatch);
        if (source_format == format::png && declares_animation(source)) return fail(decode_error::animation_unsupported);

        const com_apartment apartment{};
        if (!apartment.available()) return fail(decode_error::com_initialize_failed, apartment.result());

        ComPtr<IWICImagingFactory2> factory;
        HRESULT result = ::CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory2), reinterpret_cast<void**>(factory.GetAddressOf()));
        if (FAILED(result) || !factory) return fail(decode_error::factory_create_failed, result);

        ComPtr<IWICStream> stream;
        result = factory->CreateStream(stream.GetAddressOf());
        if (FAILED(result) || !stream) return fail(decode_error::stream_create_failed, result);

        auto* const mutable_source = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(source.data()));
        result = stream->InitializeFromMemory(mutable_source, static_cast<DWORD>(source.size()));
        if (FAILED(result)) return fail(decode_error::stream_initialize_failed, result);

        const CLSID& decoder_class = source_format == format::png ? CLSID_WICPngDecoder2 :
            source_format == format::jpeg ? CLSID_WICJpegDecoder : CLSID_WICBmpDecoder;
        ComPtr<IWICBitmapDecoder> decoder;
        result = ::CoCreateInstance(decoder_class, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICBitmapDecoder), reinterpret_cast<void**>(decoder.GetAddressOf()));
        if (FAILED(result) || !decoder) return fail(decode_error::decoder_create_failed, result);
        result = decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand);
        if (FAILED(result)) return fail(decode_error::decoder_initialize_failed, result);

        UINT frame_count = 0;
        result = decoder->GetFrameCount(&frame_count);
        if (FAILED(result)) return fail(decode_error::frame_count_failed, result);
        if (frame_count != 1) return fail(decode_error::animation_unsupported);

        ComPtr<IWICBitmapFrameDecode> frame;
        result = decoder->GetFrame(0, frame.GetAddressOf());
        if (FAILED(result) || !frame) return fail(decode_error::frame_get_failed, result);

        UINT width = 0;
        UINT height = 0;
        result = frame->GetSize(&width, &height);
        if (FAILED(result)) return fail(decode_error::size_query_failed, result);
        if (width == 0 || height == 0 || width > k_max_dimension || height > k_max_dimension) {
            return fail(decode_error::dimensions_out_of_range);
        }
        const dh::size_t pixel_count = static_cast<dh::size_t>(width) * static_cast<dh::size_t>(height);
        if (pixel_count > k_max_decoded_bytes / k_bytes_per_pixel) return fail(decode_error::dimensions_out_of_range);
        if (pixel_count > output_limit_bytes / k_bytes_per_pixel) return fail(decode_error::output_limit_exceeded);

        ComPtr<IWICFormatConverter> converter;
        result = factory->CreateFormatConverter(converter.GetAddressOf());
        if (FAILED(result) || !converter) return fail(decode_error::converter_create_failed, result);
        result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(result)) return fail(decode_error::converter_initialize_failed, result);

        decoded_image image{};
        image.width = static_cast<dh::u32>(width);
        image.height = static_cast<dh::u32>(height);
        image.rgba.resize(pixel_count * k_bytes_per_pixel);
        const UINT stride = width * static_cast<UINT>(k_bytes_per_pixel);
        result = converter->CopyPixels(nullptr, stride, static_cast<UINT>(image.rgba.size()), image.rgba.data());
        if (FAILED(result)) return fail(decode_error::pixel_copy_failed, result);
        return image;
    }

}
