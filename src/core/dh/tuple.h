// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include "core/dh/utility.h"

#include <utility>

namespace dh {

    template <typename... Ts> class tuple;

    template <> class tuple<> {
    public:
        constexpr tuple() noexcept = default;
    };

    template <typename Head, typename... Tail> class tuple<Head, Tail...> : private tuple<Tail...> {
    public:
        using head_type = Head;
        using tail_type = tuple<Tail...>;

        constexpr tuple() noexcept : tail_type(), m_head() {}

        constexpr explicit tuple(Head h, Tail... t) noexcept : tail_type(static_cast<Tail&&>(t)...), m_head(static_cast<Head&&>(h)) {}

        constexpr tuple(const tuple&) noexcept = default;
        constexpr tuple(tuple&&) noexcept = default;
        constexpr tuple& operator=(const tuple&) noexcept = default;
        constexpr tuple& operator=(tuple&&) noexcept = default;
        ~tuple() = default;

        constexpr Head& head() noexcept { return m_head; }

        constexpr const Head& head() const noexcept { return m_head; }

        constexpr tail_type& tail() noexcept { return *this; }

        constexpr const tail_type& tail() const noexcept { return *this; }

    private:
        Head m_head;
    };

    template <typename... Ts> tuple(Ts...) -> tuple<Ts...>;

    namespace detail {
        template <dh::size_t I, typename Tup> struct tuple_get;

        template <typename Head, typename... Tail> struct tuple_get<0, tuple<Head, Tail...>> {
            static constexpr Head& run(tuple<Head, Tail...>& t) noexcept { return t.head(); }

            static constexpr const Head& run(const tuple<Head, Tail...>& t) noexcept { return t.head(); }
        };

        template <dh::size_t I, typename Head, typename... Tail> struct tuple_get<I, tuple<Head, Tail...>> {
            static constexpr decltype(auto) run(tuple<Head, Tail...>& t) noexcept { return tuple_get<I - 1, tuple<Tail...>>::run(t.tail()); }

            static constexpr decltype(auto) run(const tuple<Head, Tail...>& t) noexcept { return tuple_get<I - 1, tuple<Tail...>>::run(t.tail()); }
        };
    }

    template <dh::size_t I, typename... Ts> constexpr decltype(auto) get(tuple<Ts...>& t) noexcept {
        return detail::tuple_get<I, tuple<Ts...>>::run(t);
    }

    template <dh::size_t I, typename... Ts> constexpr decltype(auto) get(const tuple<Ts...>& t) noexcept {
        return detail::tuple_get<I, tuple<Ts...>>::run(t);
    }

    template <dh::size_t I, typename Tup> struct tuple_element;

    template <typename Head, typename... Tail> struct tuple_element<0, tuple<Head, Tail...>> {
        using type = Head;
    };

    template <dh::size_t I, typename Head, typename... Tail> struct tuple_element<I, tuple<Head, Tail...>> {
        using type = typename tuple_element<I - 1, tuple<Tail...>>::type;
    };

    template <dh::size_t I, typename Tup> using tuple_element_t = typename tuple_element<I, Tup>::type;

    template <dh::size_t I, typename... Ts> constexpr decltype(auto) get(tuple<Ts...>&& t) noexcept {
        return static_cast<tuple_element_t<I, tuple<Ts...>>&&>(dh::get<I>(t));
    }

    template <dh::size_t I, typename... Ts> constexpr decltype(auto) get(const tuple<Ts...>&& t) noexcept {
        return static_cast<const tuple_element_t<I, tuple<Ts...>>&&>(dh::get<I>(t));
    }

    template <typename Tup> struct tuple_size;

    template <typename... Ts> struct tuple_size<tuple<Ts...>> {
        static constexpr dh::size_t value = sizeof...(Ts);
    };

    template <typename Tup> inline constexpr dh::size_t tuple_size_v = tuple_size<Tup>::value;

    namespace detail {
        template <typename F, typename Tup, dh::size_t... Is> constexpr decltype(auto) apply_impl(F&& f, Tup&& t, dh::index_sequence<Is...>) {
            return static_cast<F&&>(f)(get<Is>(static_cast<Tup&&>(t))...);
        }
    }

    template <typename F, typename... Ts> constexpr decltype(auto) apply(F&& f, tuple<Ts...>& t) {
        return detail::apply_impl(static_cast<F&&>(f), t, dh::make_index_sequence<sizeof...(Ts)>{});
    }

    template <typename F, typename... Ts> constexpr decltype(auto) apply(F&& f, const tuple<Ts...>& t) {
        return detail::apply_impl(static_cast<F&&>(f), t, dh::make_index_sequence<sizeof...(Ts)>{});
    }

    template <typename F, typename... Ts> constexpr decltype(auto) apply(F&& f, tuple<Ts...>&& t) {
        return detail::apply_impl(static_cast<F&&>(f), static_cast<tuple<Ts...>&&>(t), dh::make_index_sequence<sizeof...(Ts)>{});
    }

}

namespace std {

    template <typename... Ts> struct tuple_size<dh::tuple<Ts...>> : integral_constant<size_t, sizeof...(Ts)> {};

    template <size_t I, typename... Ts> struct tuple_element<I, dh::tuple<Ts...>> {
        using type = typename dh::tuple_element<I, dh::tuple<Ts...>>::type;
    };

}
