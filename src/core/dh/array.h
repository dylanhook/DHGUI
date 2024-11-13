// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include "core/dh/type_traits.h"

namespace dh {

    template <typename T, dh::size_t N> struct array final {
        static_assert(N > 0, "dh::array needs at least one element");

        using value_type = T;
        using size_type = dh::size_t;
        using difference_type = dh::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;

        T elems_[N];

        [[nodiscard]] constexpr reference operator[](size_type i) noexcept { return elems_[i]; }

        [[nodiscard]] constexpr const_reference operator[](size_type i) const noexcept { return elems_[i]; }

        [[nodiscard]] constexpr pointer data() noexcept { return elems_; }

        [[nodiscard]] constexpr const_pointer data() const noexcept { return elems_; }

        [[nodiscard]] constexpr size_type size() const noexcept { return N; }

        [[nodiscard]] constexpr bool empty() const noexcept { return false; }

        [[nodiscard]] constexpr iterator begin() noexcept { return elems_; }

        [[nodiscard]] constexpr const_iterator begin() const noexcept { return elems_; }

        [[nodiscard]] constexpr iterator end() noexcept { return elems_ + N; }

        [[nodiscard]] constexpr const_iterator end() const noexcept { return elems_ + N; }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return elems_; }

        [[nodiscard]] constexpr const_iterator cend() const noexcept { return elems_ + N; }

        [[nodiscard]] constexpr reference front() noexcept { return elems_[0]; }

        [[nodiscard]] constexpr const_reference front() const noexcept { return elems_[0]; }

        [[nodiscard]] constexpr reference back() noexcept { return elems_[N - 1]; }

        [[nodiscard]] constexpr const_reference back() const noexcept { return elems_[N - 1]; }

        constexpr void fill(const T& value) noexcept {
            for (size_type i = 0; i < N; ++i) {
                elems_[i] = value;
            }
        }
    };

    template <typename T, typename... U>
        requires(dh::is_same_v<T, U> && ...)
    array(T, U...) -> array<T, 1 + sizeof...(U)>;

}
