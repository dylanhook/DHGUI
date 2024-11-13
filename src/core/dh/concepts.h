// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "type_traits.h"

namespace dh {

    namespace detail {

        template <typename T> concept boolean_testable_impl = is_convertible_v<T, bool>;

        template <typename T> concept boolean_testable = boolean_testable_impl<T> && requires(T&& t) {
            { !static_cast<T&&>(t) } -> boolean_testable_impl;
        };

    }

    template <typename From, typename To> concept convertible_to = is_convertible_v<From, To> && requires { static_cast<To>(dh::declval<From>()); };

    template <typename T> concept equality_comparable = requires(const remove_reference_t<T>& a, const remove_reference_t<T>& b) {
        { a == b } -> detail::boolean_testable;
        { a != b } -> detail::boolean_testable;
    };

}
