// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

namespace dh {

    template <typename T, T V> struct integral_constant {
        using value_type = T;
        using type = integral_constant;

        static constexpr T value = V;

        [[nodiscard]] constexpr operator value_type() const noexcept { return value; }

        [[nodiscard]] constexpr value_type operator()() const noexcept { return value; }
    };

    template <bool B> using bool_constant = integral_constant<bool, B>;

    using true_type = bool_constant<true>;
    using false_type = bool_constant<false>;

    template <typename T> struct type_identity {
        using type = T;
    };

    template <typename T> using type_identity_t = typename type_identity<T>::type;

    template <typename...> using void_t = void;

    template <bool B, typename T, typename F> struct conditional {
        using type = T;
    };

    template <typename T, typename F> struct conditional<false, T, F> {
        using type = F;
    };

    template <bool B, typename T, typename F> using conditional_t = typename conditional<B, T, F>::type;

    template <bool B, typename T = void> struct enable_if {};

    template <typename T> struct enable_if<true, T> {
        using type = T;
    };

    template <bool B, typename T = void> using enable_if_t = typename enable_if<B, T>::type;

    template <typename A, typename B> inline constexpr bool is_same_v = false;

    template <typename A> inline constexpr bool is_same_v<A, A> = true;

    template <typename A, typename B> struct is_same : bool_constant<is_same_v<A, B>> {};

    template <typename T> struct remove_reference {
        using type = T;
    };

    template <typename T> struct remove_reference<T&> {
        using type = T;
    };

    template <typename T> struct remove_reference<T&&> {
        using type = T;
    };

    template <typename T> using remove_reference_t = typename remove_reference<T>::type;

    template <typename T> struct remove_const {
        using type = T;
    };

    template <typename T> struct remove_const<const T> {
        using type = T;
    };

    template <typename T> using remove_const_t = typename remove_const<T>::type;

    template <typename T> struct remove_cv {
        using type = T;
        template <template <typename> class Fn> using reapply = Fn<T>;
    };

    template <typename T> struct remove_cv<const T> {
        using type = T;
        template <template <typename> class Fn> using reapply = const Fn<T>;
    };

    template <typename T> struct remove_cv<volatile T> {
        using type = T;
        template <template <typename> class Fn> using reapply = volatile Fn<T>;
    };

    template <typename T> struct remove_cv<const volatile T> {
        using type = T;
        template <template <typename> class Fn> using reapply = const volatile Fn<T>;
    };

    template <typename T> using remove_cv_t = typename remove_cv<T>::type;

    template <typename T> using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;

    template <typename T> struct remove_cvref {
        using type = remove_cvref_t<T>;
    };

    template <typename T> struct remove_pointer {
        using type = T;
    };

    template <typename T> struct remove_pointer<T*> {
        using type = T;
    };

    template <typename T> struct remove_pointer<T* const> {
        using type = T;
    };

    template <typename T> struct remove_pointer<T* volatile> {
        using type = T;
    };

    template <typename T> struct remove_pointer<T* const volatile> {
        using type = T;
    };

    template <typename T> using remove_pointer_t = typename remove_pointer<T>::type;

    template <typename T> struct remove_extent {
        using type = T;
    };

    template <typename T> struct remove_extent<T[]> {
        using type = T;
    };

    template <typename T, size_t N> struct remove_extent<T[N]> {
        using type = T;
    };

    template <typename T> using remove_extent_t = typename remove_extent<T>::type;

    namespace detail {

        template <typename T, typename = void> struct add_reference {
            using lvalue = T;
            using rvalue = T;
        };

        template <typename T> struct add_reference<T, void_t<T&>> {
            using lvalue = T&;
            using rvalue = T&&;
        };

    }

    template <typename T> using add_lvalue_reference_t = typename detail::add_reference<T>::lvalue;
    template <typename T> using add_rvalue_reference_t = typename detail::add_reference<T>::rvalue;

    template <typename T> add_rvalue_reference_t<T> declval() noexcept;

    template <typename T> inline constexpr bool is_void_v = is_same_v<remove_cv_t<T>, void>;

    template <typename T> inline constexpr bool is_lvalue_reference_v = false;
    template <typename T> inline constexpr bool is_lvalue_reference_v<T&> = true;

    template <typename T> inline constexpr bool is_reference_v = false;
    template <typename T> inline constexpr bool is_reference_v<T&> = true;
    template <typename T> inline constexpr bool is_reference_v<T&&> = true;

    template <typename T> inline constexpr bool is_const_v = false;
    template <typename T> inline constexpr bool is_const_v<const T> = true;

    namespace detail {
        template <typename T> inline constexpr bool is_pointer_impl_v = false;
        template <typename T> inline constexpr bool is_pointer_impl_v<T*> = true;
    }

    template <typename T> inline constexpr bool is_pointer_v = detail::is_pointer_impl_v<remove_cv_t<T>>;

    template <typename T> inline constexpr bool is_array_v = false;
    template <typename T> inline constexpr bool is_array_v<T[]> = true;
    template <typename T, size_t N> inline constexpr bool is_array_v<T[N]> = true;

    template <typename T> inline constexpr bool is_function_v = !is_const_v<const T> && !is_reference_v<T>;

    namespace detail {
        template <typename T> inline constexpr bool is_integral_impl_v = false;
        template <> inline constexpr bool is_integral_impl_v<bool> = true;
        template <> inline constexpr bool is_integral_impl_v<char> = true;
        template <> inline constexpr bool is_integral_impl_v<signed char> = true;
        template <> inline constexpr bool is_integral_impl_v<unsigned char> = true;
        template <> inline constexpr bool is_integral_impl_v<wchar_t> = true;
        template <> inline constexpr bool is_integral_impl_v<char8_t> = true;
        template <> inline constexpr bool is_integral_impl_v<char16_t> = true;
        template <> inline constexpr bool is_integral_impl_v<char32_t> = true;
        template <> inline constexpr bool is_integral_impl_v<short> = true;
        template <> inline constexpr bool is_integral_impl_v<unsigned short> = true;
        template <> inline constexpr bool is_integral_impl_v<int> = true;
        template <> inline constexpr bool is_integral_impl_v<unsigned int> = true;
        template <> inline constexpr bool is_integral_impl_v<long> = true;
        template <> inline constexpr bool is_integral_impl_v<unsigned long> = true;
        template <> inline constexpr bool is_integral_impl_v<long long> = true;
        template <> inline constexpr bool is_integral_impl_v<unsigned long long> = true;

        template <typename T> inline constexpr bool is_floating_point_impl_v = false;
        template <> inline constexpr bool is_floating_point_impl_v<float> = true;
        template <> inline constexpr bool is_floating_point_impl_v<double> = true;
        template <> inline constexpr bool is_floating_point_impl_v<long double> = true;
    }

    template <typename T> inline constexpr bool is_integral_v = detail::is_integral_impl_v<remove_cv_t<T>>;

    template <typename T> inline constexpr bool is_floating_point_v = detail::is_floating_point_impl_v<remove_cv_t<T>>;

    template <typename T> inline constexpr bool is_arithmetic_v = is_integral_v<T> || is_floating_point_v<T>;

    template <typename T> inline constexpr bool is_enum_v = __is_enum(T);

    namespace detail {

        template <typename T, bool = is_integral_v<T>> struct sign_base {
            using unqualified = remove_cv_t<T>;
            static constexpr bool is_signed = static_cast<unqualified>(-1) < static_cast<unqualified>(0);
            static constexpr bool is_unsigned = !is_signed;
        };

        template <typename T> struct sign_base<T, false> {
            static constexpr bool is_signed = is_floating_point_v<T>;
            static constexpr bool is_unsigned = false;
        };

    }

    template <typename T> inline constexpr bool is_signed_v = detail::sign_base<T>::is_signed;
    template <typename T> inline constexpr bool is_unsigned_v = detail::sign_base<T>::is_unsigned;

    namespace detail {

        template <size_t N> struct make_unsigned_by_size;

        template <> struct make_unsigned_by_size<1> {
            template <typename> using apply = unsigned char;
        };

        template <> struct make_unsigned_by_size<2> {
            template <typename> using apply = unsigned short;
        };

        template <> struct make_unsigned_by_size<4> {
            template <typename T> using apply = conditional_t<is_same_v<T, long> || is_same_v<T, unsigned long>, unsigned long, unsigned int>;
        };

        template <> struct make_unsigned_by_size<8> {
            template <typename> using apply = unsigned long long;
        };

        template <typename T> using make_unsigned_unqualified = typename make_unsigned_by_size<sizeof(T)>::template apply<T>;

    }

    template <typename T> struct make_unsigned {
        static_assert((is_integral_v<T> && !is_same_v<remove_cv_t<T>, bool>) || is_enum_v<T>, "dh::make_unsigned needs an integer or enum, bool is not supported");
        using type = typename remove_cv<T>::template reapply<detail::make_unsigned_unqualified>;
    };

    template <typename T> using make_unsigned_t = typename make_unsigned<T>::type;

    namespace detail {

        template <typename T, typename = void> struct add_pointer {
            using type = T;
        };

        template <typename T> struct add_pointer<T, void_t<remove_reference_t<T>*>> {
            using type = remove_reference_t<T>*;
        };

    }

    template <typename T> using add_pointer_t = typename detail::add_pointer<T>::type;

    template <typename T> struct decay {
    private:
        using unreferenced = remove_reference_t<T>;

    public:
        using type = conditional_t<is_array_v<unreferenced>, add_pointer_t<remove_extent_t<unreferenced>>,
            conditional_t<is_function_v<unreferenced>, add_pointer_t<unreferenced>, remove_cv_t<unreferenced>>>;
    };

    template <typename T> using decay_t = typename decay<T>::type;

    template <typename T, typename... Args> inline constexpr bool is_constructible_v = __is_constructible(T, Args...);

    template <typename T> inline constexpr bool is_default_constructible_v = __is_constructible(T);

    template <typename T> inline constexpr bool is_copy_constructible_v = __is_constructible(T, add_lvalue_reference_t<const T>);

    template <typename T> inline constexpr bool is_move_constructible_v = __is_constructible(T, T);

    template <typename To, typename From> inline constexpr bool is_assignable_v = __is_assignable(To, From);

    template <typename T> inline constexpr bool is_copy_assignable_v = __is_assignable(add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>);

    template <typename T> inline constexpr bool is_move_assignable_v = __is_assignable(add_lvalue_reference_t<T>, T);

    template <typename T, typename... Args> inline constexpr bool is_nothrow_constructible_v = __is_nothrow_constructible(T, Args...);

    template <typename T> inline constexpr bool is_nothrow_default_constructible_v = __is_nothrow_constructible(T);

    template <typename T> inline constexpr bool is_nothrow_copy_constructible_v = __is_nothrow_constructible(T, add_lvalue_reference_t<const T>);

    template <typename T> inline constexpr bool is_nothrow_move_constructible_v = __is_nothrow_constructible(T, T);

    template <typename To, typename From> inline constexpr bool is_nothrow_assignable_v = __is_nothrow_assignable(To, From);

    template <typename T> inline constexpr bool is_nothrow_copy_assignable_v =
        __is_nothrow_assignable(add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>);

    template <typename T> inline constexpr bool is_nothrow_move_assignable_v = __is_nothrow_assignable(add_lvalue_reference_t<T>, T);

    template <typename T> inline constexpr bool is_nothrow_destructible_v = __is_nothrow_destructible(T);

    template <typename T> inline constexpr bool is_trivially_copyable_v = __is_trivially_copyable(T);

    template <typename T> inline constexpr bool is_trivially_destructible_v = __is_trivially_destructible(T);

    template <typename T> inline constexpr bool is_trivially_default_constructible_v = __is_trivially_constructible(T);

    template <typename Base, typename Derived> inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);

    template <typename From, typename To> inline constexpr bool is_convertible_v = __is_convertible_to(From, To);

    template <typename T> inline constexpr bool is_standard_layout_v = __is_standard_layout(T);
    template <typename T> inline constexpr bool is_empty_v = __is_empty(T);
    template <typename T> inline constexpr bool is_final_v = __is_final(T);

    template <typename T, typename... Args> struct is_constructible : bool_constant<is_constructible_v<T, Args...>> {};

    template <typename T> struct is_trivially_copyable : bool_constant<is_trivially_copyable_v<T>> {};

    namespace detail {

        template <typename Void, typename F, typename... Args> struct invoke_traits {
            static constexpr bool invocable = false;
            static constexpr bool nothrow_invocable = false;
        };

        template <typename F, typename... Args> struct invoke_traits<void_t<decltype(dh::declval<F>()(dh::declval<Args>()...))>, F, Args...> {
            using type = decltype(dh::declval<F>()(dh::declval<Args>()...));

            static constexpr bool invocable = true;
            static constexpr bool nothrow_invocable = noexcept(dh::declval<F>()(dh::declval<Args>()...));
        };

        template <bool Invocable, typename R, typename Traits> inline constexpr bool invoke_r_v = false;

        template <typename R, typename Traits> inline constexpr bool invoke_r_v<true, R, Traits> = is_void_v<R> || is_convertible_v<typename Traits::type, R>;

    }

    template <typename F, typename... Args> using invoke_result_t = typename detail::invoke_traits<void, F, Args...>::type;

    template <typename F, typename... Args> inline constexpr bool is_invocable_v = detail::invoke_traits<void, F, Args...>::invocable;

    template <typename F, typename... Args> inline constexpr bool is_nothrow_invocable_v = detail::invoke_traits<void, F, Args...>::nothrow_invocable;

    template <typename R, typename F, typename... Args> inline constexpr bool is_invocable_r_v =
        detail::invoke_r_v<detail::invoke_traits<void, F, Args...>::invocable, R, detail::invoke_traits<void, F, Args...>>;

}
