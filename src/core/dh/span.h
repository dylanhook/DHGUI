// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include "core/dh/array.h"
#include "core/dh/concepts.h"
#include "core/dh/type_traits.h"

namespace dh {

    template <typename T> class span final {
    public:
        using element_type = T;
        using value_type = dh::remove_cv_t<T>;
        using size_type = dh::size_t;
        using difference_type = dh::ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator = T*;
        using const_iterator = const T*;

        constexpr span() noexcept = default;
        constexpr span(const span&) noexcept = default;
        constexpr span& operator=(const span&) noexcept = default;
        constexpr span(span&&) noexcept = default;
        constexpr span& operator=(span&&) noexcept = default;
        ~span() = default;

        constexpr span(T* first, size_type count) noexcept : m_data(first), m_size(count) {}

        constexpr span(T* first, T* last) noexcept : m_data(first), m_size(static_cast<size_type>(last - first)) {}

        template <typename U>
            requires(dh::is_convertible_v<U*, T*> && !dh::is_convertible_v<U (*)[], T (*)[]>)
        span(U*, size_type) = delete;
        template <typename U>
            requires(dh::is_convertible_v<U*, T*> && !dh::is_convertible_v<U (*)[], T (*)[]>)
        span(U*, U*) = delete;

        template <dh::size_t N> constexpr span(T (&arr)[N]) noexcept : m_data(arr), m_size(N) {}

        template <typename C>
            requires(!dh::is_same_v<dh::remove_cvref_t<C>, span> && (dh::is_lvalue_reference_v<C> || dh::is_const_v<T>) && requires(C&& c) {
                            { c.data() } -> dh::convertible_to<T*>;
                            requires dh::is_convertible_v<dh::remove_pointer_t<decltype(c.data())>(*)[], T(*)[]>;
                            { c.size() };
                        })
        constexpr span(C&& c) noexcept : m_data(c.data()), m_size(static_cast<size_type>(c.size())) {}

        template <typename U>
            requires(!dh::is_same_v<U, T> && dh::is_same_v<dh::remove_const_t<T>, U>)
        constexpr span(const span<U>& other) noexcept : m_data(other.data()), m_size(other.size()) {}

        [[nodiscard]] constexpr pointer data() const noexcept { return m_data; }

        [[nodiscard]] constexpr size_type size() const noexcept { return m_size; }

        [[nodiscard]] constexpr size_type size_bytes() const noexcept { return m_size * sizeof(element_type); }

        [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

        [[nodiscard]] constexpr reference operator[](size_type i) const noexcept { return m_data[i]; }

        [[nodiscard]] constexpr reference front() const noexcept { return m_data[0]; }

        [[nodiscard]] constexpr reference back() const noexcept { return m_data[m_size - 1]; }

        [[nodiscard]] constexpr iterator begin() const noexcept { return m_data; }

        [[nodiscard]] constexpr iterator end() const noexcept { return m_data + m_size; }

        [[nodiscard]] constexpr span first(size_type n) const noexcept { return span{m_data, n < m_size ? n : m_size}; }

        [[nodiscard]] constexpr span last(size_type n) const noexcept {
            const size_type take = n < m_size ? n : m_size;
            return span{m_data + (m_size - take), take};
        }

        [[nodiscard]] constexpr span subspan(size_type offset, size_type count = size_type(-1)) const noexcept {
            const size_type off = offset < m_size ? offset : m_size;
            const size_type remaining = m_size - off;
            const size_type take = count < remaining ? count : remaining;
            return span{m_data + off, take};
        }

        [[nodiscard]] span<const dh::u8> as_bytes() const noexcept { return span<const dh::u8>{reinterpret_cast<const dh::u8*>(m_data), size_bytes()}; }

    private:
        pointer m_data{nullptr};
        size_type m_size{0};
    };

    template <typename T, dh::size_t N> span(T (&)[N]) -> span<T>;

    template <typename T> span(T*, dh::size_t) -> span<T>;

    template <typename T> span(T*, T*) -> span<T>;

}
