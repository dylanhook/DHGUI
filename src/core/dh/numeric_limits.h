// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include "core/dh/type_traits.h"

namespace dh {

    template <typename T> class numeric_limits;

    namespace detail {

        class limits_identity {
        public:
            limits_identity() = delete;
            limits_identity(const limits_identity&) = delete;
            limits_identity(limits_identity&&) = delete;
            limits_identity& operator=(const limits_identity&) = delete;
            limits_identity& operator=(limits_identity&&) = delete;
            ~limits_identity() = default;
        };

        template <typename T> class integral_limits : public limits_identity {
        public:
            static constexpr bool is_signed = dh::is_signed_v<T>;

            [[nodiscard]] static constexpr T max() noexcept {
                constexpr dh::u64 k_word_bits = sizeof(dh::u64) * 8;
                constexpr dh::u64 k_value_bits = sizeof(T) * 8 - (is_signed ? 1u : 0u);
                constexpr dh::u64 k_all_ones = ~dh::u64{0};

                return static_cast<T>(k_all_ones >> (k_word_bits - k_value_bits));
            }

            [[nodiscard]] static constexpr T lowest() noexcept {
                if constexpr (is_signed) {
                    return static_cast<T>(-static_cast<dh::i64>(max()) - 1);
                } else {
                    return T{0};
                }
            }

            [[nodiscard]] static constexpr T min() noexcept { return lowest(); }
        };

    }

    template <> class numeric_limits<dh::i8> final : public detail::integral_limits<dh::i8> {};

    template <> class numeric_limits<dh::i16> final : public detail::integral_limits<dh::i16> {};

    template <> class numeric_limits<dh::i32> final : public detail::integral_limits<dh::i32> {};

    template <> class numeric_limits<dh::i64> final : public detail::integral_limits<dh::i64> {};

    template <> class numeric_limits<dh::u8> final : public detail::integral_limits<dh::u8> {};

    template <> class numeric_limits<dh::u16> final : public detail::integral_limits<dh::u16> {};

    template <> class numeric_limits<dh::u32> final : public detail::integral_limits<dh::u32> {};

    template <> class numeric_limits<dh::u64> final : public detail::integral_limits<dh::u64> {};

    template <> class numeric_limits<float> final : public detail::limits_identity {
    public:
        static constexpr bool is_signed = true;

        [[nodiscard]] static constexpr float min() noexcept { return 1.17549435e-38f; }

        [[nodiscard]] static constexpr float max() noexcept { return 3.40282347e+38f; }

        [[nodiscard]] static constexpr float lowest() noexcept { return -3.40282347e+38f; }

        [[nodiscard]] static float infinity() noexcept { return __builtin_huge_valf(); }
    };

    template <> class numeric_limits<double> final : public detail::limits_identity {
    public:
        static constexpr bool is_signed = true;

        [[nodiscard]] static constexpr double min() noexcept { return 2.2250738585072014e-308; }

        [[nodiscard]] static constexpr double max() noexcept { return 1.7976931348623157e+308; }

        [[nodiscard]] static constexpr double lowest() noexcept { return -1.7976931348623157e+308; }

        [[nodiscard]] static double infinity() noexcept { return __builtin_huge_val(); }
    };

    static_assert(numeric_limits<dh::i8>::max() == dh::i8{127});
    static_assert(numeric_limits<dh::i8>::lowest() == dh::i8{-128});
    static_assert(numeric_limits<dh::u8>::max() == dh::u8{255});
    static_assert(numeric_limits<dh::u8>::lowest() == dh::u8{0});
    static_assert(numeric_limits<dh::i16>::max() == dh::i16{32767});
    static_assert(numeric_limits<dh::i16>::lowest() == dh::i16{-32768});
    static_assert(numeric_limits<dh::u16>::max() == dh::u16{65535});
    static_assert(numeric_limits<dh::i32>::max() == dh::i32{2147483647});
    static_assert(numeric_limits<dh::i32>::lowest() == dh::i32{-2147483647} - 1);
    static_assert(numeric_limits<dh::u32>::max() == dh::u32{4294967295u});
    static_assert(numeric_limits<dh::i64>::max() == dh::i64{9223372036854775807ll});
    static_assert(numeric_limits<dh::i64>::lowest() == dh::i64{-9223372036854775807ll} - 1);
    static_assert(numeric_limits<dh::u64>::max() == dh::u64{18446744073709551615ull});
    static_assert(numeric_limits<dh::u64>::lowest() == dh::u64{0});
    static_assert(numeric_limits<dh::i32>::is_signed && !numeric_limits<dh::u32>::is_signed);

}
