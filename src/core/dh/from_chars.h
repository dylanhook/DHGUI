// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"
#include "expected.h"
#include "string.h"

#include <xmmintrin.h>

namespace dh {

    namespace detail {

        class number_conversion_environment final {
        public:
            number_conversion_environment() noexcept : m_saved(_mm_getcsr()) {
                _mm_setcsr(_MM_MASK_MASK);
            }

            ~number_conversion_environment() noexcept { _mm_setcsr(m_saved); }
            number_conversion_environment(const number_conversion_environment&) = delete;
            number_conversion_environment& operator=(const number_conversion_environment&) = delete;
            number_conversion_environment(number_conversion_environment&&) = delete;
            number_conversion_environment& operator=(number_conversion_environment&&) = delete;

        private:
            const unsigned m_saved;
        };

    }

    enum class number_parse_error : dh::u8 {
        empty_input,
        invalid_argument,
        out_of_range,
    };

    [[nodiscard]] constexpr dh::string_view to_string(number_parse_error e) noexcept {
        switch (e) {
            case number_parse_error::empty_input:
                return "empty_input";
            case number_parse_error::invalid_argument:
                return "invalid_argument";
            case number_parse_error::out_of_range:
                return "out_of_range";
        }
        return "unknown";
    }

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i8& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i16& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i32& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i64& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u8& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u16& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u32& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u64& value, int base = 10) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, float& value) noexcept;

    [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, double& value) noexcept;

}
