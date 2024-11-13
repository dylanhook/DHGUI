// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/span.h"
#include "core/dh/string.h"

namespace dh::platform::strings {

    enum class convert_error : unsigned char {
        input_too_large,
        invalid_sequence,

        size_query_failed,
        conversion_failed,
        output_too_small,
    };

    [[nodiscard]] constexpr dh::string_view to_string(convert_error e) noexcept {
        switch (e) {
            case convert_error::input_too_large:
                return "input_too_large";
            case convert_error::invalid_sequence:
                return "invalid_sequence";
            case convert_error::size_query_failed:
                return "size_query_failed";
            case convert_error::conversion_failed:
                return "conversion_failed";
            case convert_error::output_too_small:
                return "output_too_small";
        }
        return "unknown";
    }

    [[nodiscard]] dh::expected<dh::string, convert_error> to_utf8(dh::wstring_view wide) noexcept;

    [[nodiscard]] dh::expected<dh::wstring, convert_error> to_utf16(dh::string_view narrow) noexcept;

    [[nodiscard]] dh::expected<dh::size_t, convert_error> copy_utf16(dh::string_view narrow, dh::span<wchar_t> output) noexcept;

    [[nodiscard]] dh::expected<dh::size_t, convert_error> utf8_scalar_count(dh::string_view narrow) noexcept;

    void to_lower_ascii(dh::span<wchar_t> chars) noexcept;

}
