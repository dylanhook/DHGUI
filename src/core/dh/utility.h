// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "type_traits.h"

namespace dh {

    template <typename T> [[nodiscard]] constexpr remove_reference_t<T>&& move(T&& value) noexcept {
        return static_cast<remove_reference_t<T>&&>(value);
    }

    template <typename T> [[nodiscard]] constexpr T&& forward(remove_reference_t<T>& value) noexcept {
        return static_cast<T&&>(value);
    }

    template <typename T> [[nodiscard]] constexpr T&& forward(remove_reference_t<T>&& value) noexcept {
        static_assert(!is_lvalue_reference_v<T>, "dh::forward cannot turn an rvalue into an lvalue");
        return static_cast<T&&>(value);
    }

    template <typename T, enable_if_t<is_move_constructible_v<T> && is_move_assignable_v<T>, int> = 0>
    constexpr void swap(T& a, T& b) noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>) {
        T tmp = dh::move(a);
        a = dh::move(b);
        b = dh::move(tmp);
    }

    namespace detail {

        template <typename T, typename = void> struct nothrow_swappable : false_type {};

        template <typename T> struct nothrow_swappable<T, void_t<decltype(swap(dh::declval<T&>(), dh::declval<T&>()))>> :
            bool_constant<noexcept(swap(dh::declval<T&>(), dh::declval<T&>()))> {};

    }

    template <typename T> inline constexpr bool is_nothrow_swappable_v = detail::nothrow_swappable<T>::value;

    template <typename T, T... Vs> struct integer_sequence {
        static_assert(is_integral_v<T>, "dh::integer_sequence needs an integer type");

        using value_type = T;

        [[nodiscard]] static constexpr size_t size() noexcept { return sizeof...(Vs); }
    };

    template <typename T, T N> using make_integer_sequence = __make_integer_seq<integer_sequence, T, N>;

    template <size_t... Vs> using index_sequence = integer_sequence<size_t, Vs...>;

    template <size_t N> using make_index_sequence = make_integer_sequence<size_t, N>;

}
