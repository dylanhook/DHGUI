// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include <new>

#include "in_place.h"
#include "core/dh/type_traits.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh {

    struct nullopt_t final {
        explicit constexpr nullopt_t(int) noexcept {}
    };

    inline constexpr nullopt_t nullopt{0};

    template <typename T> class optional;

    namespace detail {
        template <typename> inline constexpr bool is_optional_v = false;
        template <typename T> inline constexpr bool is_optional_v<optional<T>> = true;
    }

    template <typename T> class optional final {
    public:
        using value_type = T;

        constexpr optional() noexcept = default;

        constexpr optional(nullopt_t) noexcept : optional() {}

        constexpr optional(const optional& other) noexcept(dh::is_nothrow_copy_constructible_v<T>)
            requires dh::is_copy_constructible_v<T>
        {
            if (other.m_has_value) {
                ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(other.m_storage.value);
                m_has_value = true;
            }
        }

        constexpr optional(optional&& other) noexcept(dh::is_nothrow_move_constructible_v<T>)
            requires dh::is_move_constructible_v<T>
        {
            if (other.m_has_value) {
                ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::move(other.m_storage.value));
                m_has_value = true;
            }
        }

        template <typename U = T,
            typename = dh::enable_if_t<dh::is_constructible_v<T, U&&> && !detail::is_optional_v<dh::remove_cvref_t<U>> &&
                !dh::is_same_v<dh::remove_cvref_t<U>, in_place_t> && !dh::is_same_v<dh::remove_cvref_t<U>, nullopt_t>>>
        constexpr optional(U&& v) noexcept(dh::is_nothrow_constructible_v<T, U&&>) {
            ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::forward<U>(v));
            m_has_value = true;
        }

        template <typename... Args> constexpr explicit optional(in_place_t, Args&&... args) noexcept(dh::is_nothrow_constructible_v<T, Args&&...>) {
            ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::forward<Args>(args)...);
            m_has_value = true;
        }

        ~optional() requires dh::is_trivially_destructible_v<T> = default;

        ~optional() requires(!dh::is_trivially_destructible_v<T>) {
            if (m_has_value) {
                m_storage.value.~T();
            }
        }

        optional& operator=(nullopt_t) noexcept {
            reset();
            return *this;
        }

        optional& operator=(const optional& other) noexcept(dh::is_nothrow_copy_constructible_v<T> && dh::is_nothrow_copy_assignable_v<T>)
            requires(dh::is_copy_constructible_v<T> && dh::is_copy_assignable_v<T>)
        {
            if (this == &other) {
                return *this;
            }
            if (other.m_has_value) {
                if (m_has_value) {
                    m_storage.value = other.m_storage.value;
                } else {
                    ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(other.m_storage.value);
                    m_has_value = true;
                }
            } else {
                reset();
            }
            return *this;
        }

        optional& operator=(optional&& other) noexcept(dh::is_nothrow_move_constructible_v<T> && dh::is_nothrow_move_assignable_v<T>)
            requires(dh::is_move_constructible_v<T> && dh::is_move_assignable_v<T>)
        {
            if (this == &other) {
                return *this;
            }
            if (other.m_has_value) {
                if (m_has_value) {
                    m_storage.value = dh::move(other.m_storage.value);
                } else {
                    ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::move(other.m_storage.value));
                    m_has_value = true;
                }
            } else {
                reset();
            }
            return *this;
        }

        template <typename U = T,
            typename = dh::enable_if_t<dh::is_constructible_v<T, U&&> && dh::is_assignable_v<T&, U&&> && !detail::is_optional_v<dh::remove_cvref_t<U>>>>
        optional& operator=(U&& v) noexcept(dh::is_nothrow_constructible_v<T, U&&> && dh::is_nothrow_assignable_v<T&, U&&>) {
            if (m_has_value) {
                m_storage.value = dh::forward<U>(v);
            } else {
                ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::forward<U>(v));
                m_has_value = true;
            }
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept { return m_has_value; }

        constexpr explicit operator bool() const noexcept { return m_has_value; }

        constexpr T& value() & noexcept {
            check_engaged();
            return m_storage.value;
        }

        constexpr const T& value() const& noexcept {
            check_engaged();
            return m_storage.value;
        }

        constexpr T&& value() && noexcept {
            check_engaged();
            return dh::move(m_storage.value);
        }

        constexpr const T&& value() const&& noexcept {
            check_engaged();
            return dh::move(m_storage.value);
        }

        constexpr T& operator*() & noexcept { return m_storage.value; }

        constexpr const T& operator*() const& noexcept { return m_storage.value; }

        constexpr T* operator->() noexcept { return dh::addressof(m_storage.value); }

        constexpr const T* operator->() const noexcept { return dh::addressof(m_storage.value); }

        template <typename U> [[nodiscard]] constexpr T value_or(U&& default_value) const& {
            return m_has_value ? m_storage.value : static_cast<T>(dh::forward<U>(default_value));
        }

        template <typename U> [[nodiscard]] constexpr T value_or(U&& default_value) && {
            return m_has_value ? dh::move(m_storage.value) : static_cast<T>(dh::forward<U>(default_value));
        }

        template <typename... Args> constexpr T& emplace(Args&&... args) noexcept(dh::is_nothrow_constructible_v<T, Args&&...>) {
            reset();
            ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::forward<Args>(args)...);
            m_has_value = true;
            return m_storage.value;
        }

        constexpr void reset() noexcept {
            if (m_has_value) {
                m_storage.value.~T();
                m_has_value = false;
            }
        }

        constexpr void swap(optional& other) noexcept(dh::is_nothrow_move_constructible_v<T> && dh::is_nothrow_swappable_v<T>) {
            if (m_has_value && other.m_has_value) {
                using dh::swap;
                swap(m_storage.value, other.m_storage.value);
            } else if (m_has_value) {
                ::new (static_cast<void*>(dh::addressof(other.m_storage.value))) T(dh::move(m_storage.value));
                m_storage.value.~T();
                m_has_value = false;
                other.m_has_value = true;
            } else if (other.m_has_value) {
                ::new (static_cast<void*>(dh::addressof(m_storage.value))) T(dh::move(other.m_storage.value));
                other.m_storage.value.~T();
                m_has_value = true;
                other.m_has_value = false;
            }
        }

        template <typename U> friend constexpr bool operator==(const optional& a, const optional<U>& b) noexcept {
            if (a.has_value() != b.has_value()) {
                return false;
            }
            if (!a.has_value()) {
                return true;
            }
            return *a == *b;
        }

        template <typename U> friend constexpr bool operator==(const optional& a, const U& v) noexcept { return a.has_value() && (*a == v); }

        friend constexpr bool operator==(const optional& a, nullopt_t) noexcept { return !a.has_value(); }

    private:
        union storage {
            constexpr storage() noexcept : dummy{} {}

            ~storage() noexcept requires dh::is_trivially_destructible_v<T> = default;
            ~storage() noexcept requires(!dh::is_trivially_destructible_v<T>) {}

            unsigned char dummy;
            T value;
        };

        [[noreturn]] static void bad_optional_access() noexcept { dh::fail_fast("optional has no value", FAST_FAIL_INVALID_ARG); }

        constexpr void check_engaged() const noexcept {
            if (!m_has_value) {
                bad_optional_access();
            }
        }

        storage m_storage{};
        bool m_has_value{false};
    };

    template <typename T> optional(T) -> optional<T>;

}
