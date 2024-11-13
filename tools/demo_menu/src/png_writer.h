// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "device.h"

#include "core/dh/expected.h"
#include "core/dh/string.h"

namespace dh::demo_menu {

    enum class png_error : dh::u8 {
        com_unavailable,
        factory_failed,
        stream_failed,
        encoder_failed,
        frame_failed,
        pixels_failed,
        commit_failed,
    };

    [[nodiscard]] dh::string_view to_string(png_error error) noexcept;

    struct png_failure final {
        png_failure(png_error failure_cause, HRESULT result) noexcept : cause(failure_cause), hr(result) {}
        ~png_failure() = default;
        png_failure(const png_failure&) noexcept = default;
        png_failure& operator=(const png_failure&) noexcept = default;
        png_failure(png_failure&&) noexcept = default;
        png_failure& operator=(png_failure&&) noexcept = default;

        png_error cause;
        HRESULT hr;
    };

    [[nodiscard]] dh::expected<void, png_failure> write_png(const frame_pixels& pixels, dh::wstring_view path) noexcept;

}
