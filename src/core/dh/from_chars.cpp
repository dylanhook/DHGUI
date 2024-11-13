// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "from_chars.h"

#include "core/dh/numeric_limits.h"

namespace dh {

    namespace {

        constexpr dh::u64 k_max_mantissa_before_shift = (dh::numeric_limits<dh::u64>::max() - 9) / 10;

        constexpr int k_exponent_accumulator_cap = 100000;

        [[nodiscard]] constexpr dh::u8 digit_value(char c) noexcept {
            if (c >= '0' && c <= '9') {
                return static_cast<dh::u8>(c - '0');
            }
            if (c >= 'a' && c <= 'z') {
                return static_cast<dh::u8>(c - 'a' + 10);
            }
            if (c >= 'A' && c <= 'Z') {
                return static_cast<dh::u8>(c - 'A' + 10);
            }
            return 0xffu;
        }

        struct u64_scan_result final {
            dh::u64 value;
            const char* cursor;
            bool overflow;
            bool any_digits;
        };

        [[nodiscard]] u64_scan_result scan_u64(const char* first, const char* last, int base) noexcept {
            const dh::u64 b = static_cast<dh::u64>(base);
            const dh::u64 max_pre = dh::numeric_limits<dh::u64>::max() / b;
            const dh::u64 max_last = dh::numeric_limits<dh::u64>::max() % b;

            u64_scan_result out{};
            out.value = 0;
            out.cursor = first;
            out.overflow = false;
            out.any_digits = false;

            for (; out.cursor != last; ++out.cursor) {
                const dh::u8 d = digit_value(*out.cursor);
                if (d >= static_cast<dh::u8>(base)) {
                    break;
                }
                out.any_digits = true;
                if (out.overflow) {
                    continue;
                }
                if (out.value > max_pre || (out.value == max_pre && static_cast<dh::u64>(d) > max_last)) {
                    out.overflow = true;
                    continue;
                }
                out.value = out.value * b + static_cast<dh::u64>(d);
            }
            return out;
        }

        template <typename Unsigned>
        [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> parse_unsigned(const char* first, const char* last, Unsigned& dst, int base) noexcept {
            constexpr dh::u64 max_value = static_cast<dh::u64>(dh::numeric_limits<Unsigned>::max());
            if (first == last) {
                return unexpected{number_parse_error::empty_input};
            }
            if (base < 2 || base > 36) {
                return unexpected{number_parse_error::invalid_argument};
            }
            if (*first == '-') {
                return unexpected{number_parse_error::invalid_argument};
            }
            const char* cursor = first;
            if (*cursor == '+') {
                ++cursor;
                if (cursor == last) {
                    return unexpected{number_parse_error::invalid_argument};
                }
            }
            const u64_scan_result scan = scan_u64(cursor, last, base);
            if (!scan.any_digits) {
                return unexpected{number_parse_error::invalid_argument};
            }
            if (scan.overflow || scan.value > max_value) {
                return unexpected{number_parse_error::out_of_range};
            }
            dst = static_cast<Unsigned>(scan.value);
            return static_cast<dh::ptrdiff_t>(scan.cursor - first);
        }

        template <typename Signed>
        [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> parse_signed(const char* first, const char* last, Signed& dst, int base) noexcept {
            constexpr dh::u64 positive_max = static_cast<dh::u64>(dh::numeric_limits<Signed>::max());
            constexpr dh::u64 negative_max = positive_max + 1;
            if (first == last) {
                return unexpected{number_parse_error::empty_input};
            }
            if (base < 2 || base > 36) {
                return unexpected{number_parse_error::invalid_argument};
            }
            const char* cursor = first;
            bool negative = false;
            if (*cursor == '+' || *cursor == '-') {
                negative = (*cursor == '-');
                ++cursor;
                if (cursor == last) {
                    return unexpected{number_parse_error::invalid_argument};
                }
            }
            const u64_scan_result scan = scan_u64(cursor, last, base);
            if (!scan.any_digits) {
                return unexpected{number_parse_error::invalid_argument};
            }
            if (scan.overflow) {
                return unexpected{number_parse_error::out_of_range};
            }
            if (negative) {
                if (scan.value > negative_max) {
                    return unexpected{number_parse_error::out_of_range};
                }

                const dh::u64 neg_bits = static_cast<dh::u64>(0) - scan.value;
                dst = static_cast<Signed>(neg_bits);
            } else {
                if (scan.value > positive_max) {
                    return unexpected{number_parse_error::out_of_range};
                }
                dst = static_cast<Signed>(scan.value);
            }
            return static_cast<dh::ptrdiff_t>(scan.cursor - first);
        }

        template <typename Float>
        [[nodiscard]] expected<dh::ptrdiff_t, number_parse_error> parse_float(const char* first, const char* last, Float& dst) noexcept {
            if (first == last) {
                return unexpected{number_parse_error::empty_input};
            }
            const char* cursor = first;
            bool negative = false;
            if (*cursor == '+' || *cursor == '-') {
                negative = (*cursor == '-');
                ++cursor;
                if (cursor == last) {
                    return unexpected{number_parse_error::invalid_argument};
                }
            }

            dh::u64 mantissa = 0;
            dh::i64 mantissa_exp = 0;
            bool any_digits = false;
            bool mantissa_full = false;

            while (cursor != last) {
                const char c = *cursor;
                if (c < '0' || c > '9') {
                    break;
                }
                any_digits = true;
                if (!mantissa_full) {
                    if (mantissa > k_max_mantissa_before_shift) {
                        mantissa_full = true;
                        ++mantissa_exp;
                    } else {
                        mantissa = mantissa * 10ull + static_cast<dh::u64>(c - '0');
                    }
                } else {
                    ++mantissa_exp;
                }
                ++cursor;
            }

            if (cursor != last && *cursor == '.') {
                ++cursor;
                while (cursor != last) {
                    const char c = *cursor;
                    if (c < '0' || c > '9') {
                        break;
                    }
                    any_digits = true;
                    if (!mantissa_full) {
                        if (mantissa > k_max_mantissa_before_shift) {
                            mantissa_full = true;
                        } else {
                            mantissa = mantissa * 10ull + static_cast<dh::u64>(c - '0');
                            --mantissa_exp;
                        }
                    }

                    ++cursor;
                }
            }

            if (!any_digits) {
                return unexpected{number_parse_error::invalid_argument};
            }

            int explicit_exp = 0;
            bool exp_negative = false;
            if (cursor != last && (*cursor == 'e' || *cursor == 'E')) {
                const char* exp_start = cursor;
                ++cursor;
                if (cursor != last && (*cursor == '+' || *cursor == '-')) {
                    exp_negative = (*cursor == '-');
                    ++cursor;
                }
                bool exp_any = false;
                while (cursor != last) {
                    const char c = *cursor;
                    if (c < '0' || c > '9') {
                        break;
                    }
                    exp_any = true;
                    if (explicit_exp < k_exponent_accumulator_cap) {
                        explicit_exp = explicit_exp * 10 + (c - '0');
                    }
                    ++cursor;
                }
                if (!exp_any) {
                    cursor = exp_start;
                }
            }

            if (mantissa == 0) {
                dst = negative ? static_cast<Float>(-0.0) : static_cast<Float>(0.0);
                return static_cast<dh::ptrdiff_t>(cursor - first);
            }

            const dh::i64 total_exp = mantissa_exp + (exp_negative ? -explicit_exp : explicit_exp);

            double scaled = static_cast<double>(mantissa);
            dh::i64 e_abs = total_exp < 0 ? -total_exp : total_exp;
            double factor = 10.0;
            while (e_abs != 0) {
                if ((e_abs & 1) != 0) {
                    if (total_exp < 0) {
                        scaled /= factor;
                    } else {
                        scaled *= factor;
                    }
                }
                e_abs >>= 1;
                if (e_abs != 0) {
                    factor *= factor;
                }
            }
            if (negative) {
                scaled = -scaled;
            }

            const double abs_scaled = scaled < 0.0 ? -scaled : scaled;
            constexpr double k_maximum = static_cast<double>(dh::numeric_limits<Float>::max());
            if (abs_scaled > k_maximum) {
                if constexpr (dh::is_same_v<Float, float>) {
                    constexpr double k_overflow_midpoint = k_maximum + 0x1p103;
                    if (abs_scaled < k_overflow_midpoint) {
                        dst = negative ? -dh::numeric_limits<Float>::max() : dh::numeric_limits<Float>::max();
                        return static_cast<dh::ptrdiff_t>(cursor - first);
                    }
                }
                return unexpected{number_parse_error::out_of_range};
            }

            if (total_exp < 0 && mantissa != 0 && static_cast<Float>(scaled) == static_cast<Float>(0.0)) {
                return unexpected{number_parse_error::out_of_range};
            }

            dst = static_cast<Float>(scaled);
            return static_cast<dh::ptrdiff_t>(cursor - first);
        }

    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i8& value, int base) noexcept {
        return parse_signed<dh::i8>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i16& value, int base) noexcept {
        return parse_signed<dh::i16>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i32& value, int base) noexcept {
        return parse_signed<dh::i32>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::i64& value, int base) noexcept {
        return parse_signed<dh::i64>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u8& value, int base) noexcept {
        return parse_unsigned<dh::u8>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u16& value, int base) noexcept {
        return parse_unsigned<dh::u16>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u32& value, int base) noexcept {
        return parse_unsigned<dh::u32>(first, last, value, base);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, dh::u64& value, int base) noexcept {
        return parse_unsigned<dh::u64>(first, last, value, base);
    }

#pragma fenv_access(on)
    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, float& value) noexcept {
        const detail::number_conversion_environment environment;
        return parse_float<float>(first, last, value);
    }

    expected<dh::ptrdiff_t, number_parse_error> from_chars(const char* first, const char* last, double& value) noexcept {
        const detail::number_conversion_environment environment;
        return parse_float<double>(first, last, value);
    }
#pragma fenv_access(off)

}
