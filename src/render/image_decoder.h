// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/span.h"
#include "core/dh/string.h"
#include "core/dh/vector.h"

#include "core/dh/memory.h"

namespace dh::render::image {

    enum class format : dh::u8 { png, jpeg, bmp };

    inline constexpr dh::size_t k_max_source_bytes = 4 * 1024 * 1024;
    inline constexpr dh::u32 k_max_dimension = 2048;
    inline constexpr dh::size_t k_bytes_per_pixel = 4;
    inline constexpr dh::size_t k_max_decoded_bytes =
        static_cast<dh::size_t>(k_max_dimension) * k_max_dimension * k_bytes_per_pixel;

    enum class decode_error : dh::u8 {
        source_empty,
        source_too_large,
        format_invalid,
        com_initialize_failed,
        factory_create_failed,
        stream_create_failed,
        stream_initialize_failed,
        decoder_create_failed,
        decoder_initialize_failed,
        format_mismatch,
        frame_count_failed,
        animation_unsupported,
        frame_get_failed,
        size_query_failed,
        dimensions_out_of_range,
        output_limit_exceeded,
        converter_create_failed,
        converter_initialize_failed,
        pixel_copy_failed,
    };

    [[nodiscard]] dh::string to_string(decode_error error) noexcept;

    struct decode_failure final {
    public:
        decode_failure(decode_error cause_in, long hresult_in = 0) noexcept : cause(cause_in), hresult(hresult_in) {}
        ~decode_failure() = default;
        decode_failure(const decode_failure&) = default;
        decode_failure& operator=(const decode_failure&) = default;
        decode_failure(decode_failure&&) noexcept = default;
        decode_failure& operator=(decode_failure&&) noexcept = default;

        decode_error cause;
        long hresult;
    };

    struct decoded_image final {
    public:
        decoded_image() = default;
        ~decoded_image() = default;
        decoded_image(const decoded_image&) = delete;
        decoded_image& operator=(const decoded_image&) = delete;
        decoded_image(decoded_image&&) noexcept = default;
        decoded_image& operator=(decoded_image&&) noexcept = default;

        dh::u32 width{0};
        dh::u32 height{0};
        dh::vector<dh::u8> rgba{};
    };

    [[nodiscard]] dh::expected<decoded_image, decode_failure> decode(dh::span<const dh::u8> source, format source_format, dh::size_t output_limit_bytes) noexcept;

}
