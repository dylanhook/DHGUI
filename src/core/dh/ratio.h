// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

namespace dh {

    namespace detail {

        [[nodiscard]] constexpr i64 ratio_gcd(i64 a, i64 b) noexcept {
            while (b != 0) {
                const i64 t = b;
                b = a % b;
                a = t;
            }
            return a;
        }

        [[nodiscard]] constexpr i64 ratio_abs(i64 v) noexcept {
            return v < 0 ? -v : v;
        }

        [[nodiscard]] constexpr i64 ratio_sign(i64 v) noexcept {
            return v < 0 ? -1 : 1;
        }

    }

    template <i64 Num, i64 Den = 1> struct ratio final {
        static_assert(Num != (-9223372036854775807LL - 1) && Den != (-9223372036854775807LL - 1), "dh::ratio cannot use the minimum i64 value");
        static_assert(Den != 0, "dh::ratio denominator cannot be zero");

        static constexpr i64 num = Num * detail::ratio_sign(Den) / detail::ratio_gcd(detail::ratio_abs(Num), detail::ratio_abs(Den));
        static constexpr i64 den = detail::ratio_abs(Den) / detail::ratio_gcd(detail::ratio_abs(Num), detail::ratio_abs(Den));

        using type = ratio<num, den>;
    };

    namespace detail {
        template <typename R1, typename R2> struct ratio_quotient final {
            static_assert(R2::num != 0, "cannot divide by a zero ratio");
            static constexpr i64 numerator_gcd = ratio_gcd(ratio_abs(R1::num), ratio_abs(R2::num));
            static constexpr i64 denominator_gcd = ratio_gcd(R1::den, R2::den);
            using type = typename ratio<(R1::num / numerator_gcd) * (R2::den / denominator_gcd), (R1::den / denominator_gcd) * (R2::num / numerator_gcd)>::type;
        };
    }

    template <typename R1, typename R2> using ratio_divide = typename detail::ratio_quotient<R1, R2>::type;

    using nano = ratio<1, 1000000000>;
    using micro = ratio<1, 1000000>;
    using milli = ratio<1, 1000>;

}
