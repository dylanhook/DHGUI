// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "png_writer.h"

#include "core/dh/vector.h"

#include <wincodec.h>
#include <wrl/client.h>

namespace dh::demo_menu {

    dh::string_view to_string(png_error error) noexcept {
        switch (error) {
            case png_error::com_unavailable: return "com_unavailable";
            case png_error::factory_failed: return "factory_failed";
            case png_error::stream_failed: return "stream_failed";
            case png_error::encoder_failed: return "encoder_failed";
            case png_error::frame_failed: return "frame_failed";
            case png_error::pixels_failed: return "pixels_failed";
            case png_error::commit_failed: return "commit_failed";
        }
        return "unknown_png_error";
    }

    dh::expected<void, png_failure> write_png(const frame_pixels& pixels, dh::wstring_view path) noexcept {
        using Microsoft::WRL::ComPtr;
        constexpr UINT k_bytes_per_pixel = 4;

        const dh::wstring terminated{path};

        ComPtr<IWICImagingFactory> factory;
        if (const HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)); FAILED(hr))
            return dh::unexpected{png_failure{hr == CO_E_NOTINITIALIZED ? png_error::com_unavailable : png_error::factory_failed, hr}};

        ComPtr<IWICStream> stream;
        if (const HRESULT hr = factory->CreateStream(&stream); FAILED(hr)) return dh::unexpected{png_failure{png_error::stream_failed, hr}};
        if (const HRESULT hr = stream->InitializeFromFilename(terminated.c_str(), GENERIC_WRITE); FAILED(hr))
            return dh::unexpected{png_failure{png_error::stream_failed, hr}};

        ComPtr<IWICBitmapEncoder> encoder;
        if (const HRESULT hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder); FAILED(hr))
            return dh::unexpected{png_failure{png_error::encoder_failed, hr}};
        if (const HRESULT hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache); FAILED(hr))
            return dh::unexpected{png_failure{png_error::encoder_failed, hr}};

        ComPtr<IWICBitmapFrameEncode> frame;
        if (const HRESULT hr = encoder->CreateNewFrame(&frame, nullptr); FAILED(hr)) return dh::unexpected{png_failure{png_error::frame_failed, hr}};
        if (const HRESULT hr = frame->Initialize(nullptr); FAILED(hr)) return dh::unexpected{png_failure{png_error::frame_failed, hr}};
        if (const HRESULT hr = frame->SetSize(pixels.width, pixels.height); FAILED(hr)) return dh::unexpected{png_failure{png_error::frame_failed, hr}};

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (const HRESULT hr = frame->SetPixelFormat(&format); FAILED(hr) || format != GUID_WICPixelFormat32bppBGRA)
            return dh::unexpected{png_failure{png_error::pixels_failed, FAILED(hr) ? hr : E_UNEXPECTED}};

        dh::vector<dh::u8> bgra{pixels.rgba};
        for (dh::size_t i = 0; i + k_bytes_per_pixel <= bgra.size(); i += k_bytes_per_pixel) {
            const dh::u8 red = bgra[i];
            bgra[i] = bgra[i + 2];
            bgra[i + 2] = red;
        }

        const UINT stride = pixels.width * k_bytes_per_pixel;
        const UINT bytes = stride * pixels.height;
        if (const HRESULT hr = frame->WritePixels(pixels.height, stride, bytes, bgra.data()); FAILED(hr))
            return dh::unexpected{png_failure{png_error::pixels_failed, hr}};
        if (const HRESULT hr = frame->Commit(); FAILED(hr)) return dh::unexpected{png_failure{png_error::commit_failed, hr}};
        if (const HRESULT hr = encoder->Commit(); FAILED(hr)) return dh::unexpected{png_failure{png_error::commit_failed, hr}};
        return {};
    }

}
