// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include <new>

#include "in_place.h"
#include "core/dh/type_traits.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh {

    struct unexpect_t final {
        explicit unexpect_t() = default;
    };

    inline constexpr unexpect_t unexpect{};

    template <typename T, typename E> class expected;

    template <typename E> class unexpected;

    namespace detail {

        template <typename U> struct is_unexpected : dh::false_type {};

        template <typename G> struct is_unexpected<unexpected<G>> : dh::true_type {};

        template <typename U> inline constexpr bool is_unexpected_v = is_unexpected<dh::remove_cvref_t<U>>::value;

        template <typename U> struct is_expected : dh::false_type {};

        template <typename T, typename E> struct is_expected<expected<T, E>> : dh::true_type {};

        template <typename U> inline constexpr bool is_expected_v = is_expected<dh::remove_cvref_t<U>>::value;

        [[noreturn]] inline void bad_access() noexcept {
            dh::fail_fast("expected accessed as the wrong type", FAST_FAIL_INVALID_ARG);
        }

    }

    template <typename E> class unexpected final {
    public:
        unexpected() = delete;

        template <typename Err = E,
            typename = dh::enable_if_t<!dh::is_same_v<dh::remove_cvref_t<Err>, unexpected> && !dh::is_same_v<dh::remove_cvref_t<Err>, in_place_t> &&
                dh::is_constructible_v<E, Err>>>
        constexpr explicit unexpected(Err&& err) noexcept(dh::is_nothrow_constructible_v<E, Err>) : m_error(dh::forward<Err>(err)) {}

        template <typename... Args, typename = dh::enable_if_t<dh::is_constructible_v<E, Args...>>>
        constexpr explicit unexpected(in_place_t, Args&&... args) noexcept(dh::is_nothrow_constructible_v<E, Args...>) : m_error(dh::forward<Args>(args)...) {}

        constexpr unexpected(const unexpected&) = default;
        constexpr unexpected(unexpected&&) noexcept = default;
        constexpr unexpected& operator=(const unexpected&) = default;
        constexpr unexpected& operator=(unexpected&&) noexcept = default;
        ~unexpected() = default;

        [[nodiscard]] constexpr E& error() & noexcept { return m_error; }

        [[nodiscard]] constexpr const E& error() const& noexcept { return m_error; }

        [[nodiscard]] constexpr E&& error() && noexcept { return static_cast<E&&>(m_error); }

        [[nodiscard]] constexpr const E&& error() const&& noexcept { return static_cast<const E&&>(m_error); }

        template <typename G> friend constexpr bool operator==(const unexpected& a, const unexpected<G>& b) noexcept { return a.m_error == b.error(); }

    private:
        E m_error;
    };

    template <typename E> unexpected(E) -> unexpected<E>;

    template <typename T, typename E> class expected final {
    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        template <typename U> using rebind = expected<U, E>;

        constexpr expected() noexcept(dh::is_nothrow_default_constructible_v<T>)
            requires dh::is_default_constructible_v<T>
            : m_has_value(true) {
            ::new (static_cast<void*>(value_ptr())) T();
        }

        constexpr expected(const expected& other) noexcept(dh::is_nothrow_copy_constructible_v<T> && dh::is_nothrow_copy_constructible_v<E>)
            requires dh::is_copy_constructible_v<T> && dh::is_copy_constructible_v<E>
            : m_has_value(other.m_has_value) {
            if (m_has_value) {
                ::new (static_cast<void*>(value_ptr())) T(*other.value_ptr());
            } else {
                ::new (static_cast<void*>(error_ptr())) E(*other.error_ptr());
            }
        }

        constexpr expected(expected&& other) noexcept(dh::is_nothrow_move_constructible_v<T> && dh::is_nothrow_move_constructible_v<E>)
            requires dh::is_move_constructible_v<T> && dh::is_move_constructible_v<E>
            : m_has_value(other.m_has_value) {
            if (m_has_value) {
                ::new (static_cast<void*>(value_ptr())) T(static_cast<T&&>(*other.value_ptr()));
            } else {
                ::new (static_cast<void*>(error_ptr())) E(static_cast<E&&>(*other.error_ptr()));
            }
        }

        template <typename U = T,
            typename = dh::enable_if_t<!detail::is_expected_v<U> && !dh::is_same_v<dh::remove_cvref_t<U>, in_place_t> &&
                !dh::is_same_v<dh::remove_cvref_t<U>, unexpect_t> && !detail::is_unexpected_v<U> && dh::is_constructible_v<T, U>>>
        constexpr expected(U&& v) noexcept(dh::is_nothrow_constructible_v<T, U>) : m_has_value(true) {
            ::new (static_cast<void*>(value_ptr())) T(dh::forward<U>(v));
        }

        template <typename G, typename = dh::enable_if_t<dh::is_constructible_v<E, const G&>>>
        constexpr expected(const unexpected<G>& e) noexcept(dh::is_nothrow_constructible_v<E, const G&>) : m_has_value(false) {
            ::new (static_cast<void*>(error_ptr())) E(e.error());
        }

        template <typename G, typename = dh::enable_if_t<dh::is_constructible_v<E, G>>>
        constexpr expected(unexpected<G>&& e) noexcept(dh::is_nothrow_constructible_v<E, G>) : m_has_value(false) {
            ::new (static_cast<void*>(error_ptr())) E(static_cast<G&&>(e.error()));
        }

        template <typename... Args, typename = dh::enable_if_t<dh::is_constructible_v<T, Args...>>>
        constexpr explicit expected(in_place_t, Args&&... args) noexcept(dh::is_nothrow_constructible_v<T, Args...>) : m_has_value(true) {
            ::new (static_cast<void*>(value_ptr())) T(dh::forward<Args>(args)...);
        }

        template <typename... Args, typename = dh::enable_if_t<dh::is_constructible_v<E, Args...>>>
        constexpr explicit expected(unexpect_t, Args&&... args) noexcept(dh::is_nothrow_constructible_v<E, Args...>) : m_has_value(false) {
            ::new (static_cast<void*>(error_ptr())) E(dh::forward<Args>(args)...);
        }

        ~expected() requires(dh::is_trivially_destructible_v<T> && dh::is_trivially_destructible_v<E>) = default;
        ~expected() requires(!(dh::is_trivially_destructible_v<T> && dh::is_trivially_destructible_v<E>)) { destroy_active(); }

        constexpr expected& operator=(const expected& other) noexcept(dh::is_nothrow_copy_constructible_v<T> && dh::is_nothrow_copy_assignable_v<T> && dh::is_nothrow_copy_constructible_v<E> && dh::is_nothrow_copy_assignable_v<E> && dh::is_nothrow_move_constructible_v<T> && dh::is_nothrow_move_constructible_v<E>)
            requires dh::is_copy_assignable_v<T> && dh::is_copy_constructible_v<T> && dh::is_copy_assignable_v<E> && dh::is_copy_constructible_v<E> &&
            (dh::is_nothrow_move_constructible_v<T> || dh::is_nothrow_move_constructible_v<E>)
        {
            if (this == &other) {
                return *this;
            }
            if (m_has_value && other.m_has_value) {
                *value_ptr() = *other.value_ptr();
            } else if (!m_has_value && !other.m_has_value) {
                *error_ptr() = *other.error_ptr();
            } else if (m_has_value) {
                replace_member(error_ptr(), value_ptr(), *other.error_ptr());
                m_has_value = false;
            } else {
                replace_member(value_ptr(), error_ptr(), *other.value_ptr());
                m_has_value = true;
            }
            return *this;
        }

        constexpr expected& operator=(expected&& other) noexcept(dh::is_nothrow_move_constructible_v<T> && dh::is_nothrow_move_assignable_v<T> && dh::is_nothrow_move_constructible_v<E> && dh::is_nothrow_move_assignable_v<E>)
            requires dh::is_move_assignable_v<T> && dh::is_move_constructible_v<T> && dh::is_move_assignable_v<E> && dh::is_move_constructible_v<E> &&
            (dh::is_nothrow_move_constructible_v<T> || dh::is_nothrow_move_constructible_v<E>)
        {
            if (this == &other) {
                return *this;
            }
            if (m_has_value && other.m_has_value) {
                *value_ptr() = static_cast<T&&>(*other.value_ptr());
            } else if (!m_has_value && !other.m_has_value) {
                *error_ptr() = static_cast<E&&>(*other.error_ptr());
            } else if (m_has_value) {
                replace_member(error_ptr(), value_ptr(), static_cast<E&&>(*other.error_ptr()));
                m_has_value = false;
            } else {
                replace_member(value_ptr(), error_ptr(), static_cast<T&&>(*other.value_ptr()));
                m_has_value = true;
            }
            return *this;
        }

        template <typename U = T,
            typename = dh::enable_if_t<!detail::is_expected_v<U> && !detail::is_unexpected_v<U> && dh::is_constructible_v<T, U> && dh::is_assignable_v<T&, U> &&
                (dh::is_nothrow_move_constructible_v<T> || dh::is_nothrow_move_constructible_v<E>)>>
        constexpr expected& operator=(U&& v) noexcept(dh::is_nothrow_assignable_v<T&, U> && dh::is_nothrow_constructible_v<T, U> && dh::is_nothrow_move_constructible_v<T>) {
            if (m_has_value) {
                *value_ptr() = dh::forward<U>(v);
            } else {
                replace_member(value_ptr(), error_ptr(), dh::forward<U>(v));
                m_has_value = true;
            }
            return *this;
        }

        template <typename G,
            typename = dh::enable_if_t<dh::is_constructible_v<E, const G&> && dh::is_assignable_v<E&, const G&> &&
                (dh::is_nothrow_move_constructible_v<T> || dh::is_nothrow_move_constructible_v<E>)>>
        constexpr expected& operator=(const unexpected<G>& e) noexcept(dh::is_nothrow_constructible_v<E, const G&> && dh::is_nothrow_assignable_v<E&, const G&> && dh::is_nothrow_move_constructible_v<E>) {
            if (m_has_value) {
                replace_member(error_ptr(), value_ptr(), e.error());
                m_has_value = false;
            } else {
                *error_ptr() = e.error();
            }
            return *this;
        }

        template <typename G,
            typename = dh::enable_if_t<dh::is_constructible_v<E, G> && dh::is_assignable_v<E&, G> &&
                (dh::is_nothrow_move_constructible_v<T> || dh::is_nothrow_move_constructible_v<E>)>>
        constexpr expected& operator=(unexpected<G>&& e) noexcept(dh::is_nothrow_constructible_v<E, G> && dh::is_nothrow_assignable_v<E&, G> && dh::is_nothrow_move_constructible_v<E>) {
            if (m_has_value) {
                replace_member(error_ptr(), value_ptr(), static_cast<G&&>(e.error()));
                m_has_value = false;
            } else {
                *error_ptr() = static_cast<G&&>(e.error());
            }
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept { return m_has_value; }

        constexpr explicit operator bool() const noexcept { return m_has_value; }

        [[nodiscard]] constexpr T& value() & noexcept {
            if (!m_has_value) {
                detail::bad_access();
            }
            return *value_ptr();
        }

        [[nodiscard]] constexpr const T& value() const& noexcept {
            if (!m_has_value) {
                detail::bad_access();
            }
            return *value_ptr();
        }

        [[nodiscard]] constexpr T&& value() && noexcept {
            if (!m_has_value) {
                detail::bad_access();
            }
            return static_cast<T&&>(*value_ptr());
        }

        [[nodiscard]] constexpr const T&& value() const&& noexcept {
            if (!m_has_value) {
                detail::bad_access();
            }
            return static_cast<const T&&>(*value_ptr());
        }

        [[nodiscard]] constexpr E& error() & noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return *error_ptr();
        }

        [[nodiscard]] constexpr const E& error() const& noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return *error_ptr();
        }

        [[nodiscard]] constexpr E&& error() && noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return static_cast<E&&>(*error_ptr());
        }

        [[nodiscard]] constexpr const E&& error() const&& noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return static_cast<const E&&>(*error_ptr());
        }

        constexpr T& operator*() & noexcept { return *value_ptr(); }

        constexpr const T& operator*() const& noexcept { return *value_ptr(); }

        constexpr T&& operator*() && noexcept { return static_cast<T&&>(*value_ptr()); }

        constexpr const T&& operator*() const&& noexcept { return static_cast<const T&&>(*value_ptr()); }

        constexpr T* operator->() noexcept { return value_ptr(); }

        constexpr const T* operator->() const noexcept { return value_ptr(); }

        template <typename... Args>
            requires dh::is_nothrow_constructible_v<T, Args...>
        constexpr T& emplace(Args&&... args) noexcept {
            destroy_active();
            ::new (static_cast<void*>(value_ptr())) T(dh::forward<Args>(args)...);
            m_has_value = true;
            return *value_ptr();
        }

        template <typename U, typename G> friend constexpr bool operator==(const expected& a, const expected<U, G>& b) noexcept {
            if (a.has_value() != b.has_value()) {
                return false;
            }
            if (a.has_value()) {
                return *a == *b;
            }
            return a.error() == b.error();
        }

        template <typename U, typename = dh::enable_if_t<!detail::is_expected_v<U> && !detail::is_unexpected_v<U>>>
        friend constexpr bool operator==(const expected& a, const U& v) noexcept {
            return a.has_value() && *a == v;
        }

        template <typename G> friend constexpr bool operator==(const expected& a, const unexpected<G>& e) noexcept {
            return !a.has_value() && a.error() == e.error();
        }

    private:
        template <typename New, typename Old, typename Arg> static constexpr void replace_member(New* fresh, Old* old, Arg&& argument) {
            New incoming(dh::forward<Arg>(argument));
            if constexpr (dh::is_nothrow_move_constructible_v<New>) {
                old->~Old();
                ::new (static_cast<void*>(fresh)) New(dh::move(incoming));
            } else {
                static_assert(dh::is_nothrow_move_constructible_v<Old>, "expected needs a nothrow move to restore the old value");

                // restore the old value if constructing its replacement throws.
                struct rollback final {
                    Old* destination;
                    Old saved;

                    constexpr explicit rollback(Old* value) noexcept : destination(value), saved(dh::move(*value)) { value->~Old(); }

                    constexpr ~rollback() {
                        if (destination != nullptr) {
                            ::new (static_cast<void*>(destination)) Old(dh::move(saved));
                        }
                    }

                    rollback(const rollback&) = delete;
                    rollback& operator=(const rollback&) = delete;
                    rollback(rollback&&) = delete;
                    rollback& operator=(rollback&&) = delete;
                } restore{old};

                ::new (static_cast<void*>(fresh)) New(dh::move(incoming));
                restore.destination = nullptr;
            }
        }

        union storage_t {
            constexpr storage_t() noexcept {}

            ~storage_t() noexcept requires(dh::is_trivially_destructible_v<T> && dh::is_trivially_destructible_v<E>) = default;
            ~storage_t() noexcept requires(!(dh::is_trivially_destructible_v<T> && dh::is_trivially_destructible_v<E>)) {}

            T m_value;
            E m_error;
        };

        constexpr T* value_ptr() noexcept { return dh::addressof(m_storage.m_value); }

        constexpr const T* value_ptr() const noexcept { return dh::addressof(m_storage.m_value); }

        constexpr E* error_ptr() noexcept { return dh::addressof(m_storage.m_error); }

        constexpr const E* error_ptr() const noexcept { return dh::addressof(m_storage.m_error); }

        constexpr void destroy_active() noexcept {
            if (m_has_value) {
                if constexpr (!dh::is_trivially_destructible_v<T>) {
                    value_ptr()->~T();
                }
            } else {
                if constexpr (!dh::is_trivially_destructible_v<E>) {
                    error_ptr()->~E();
                }
            }
        }

        storage_t m_storage;
        bool m_has_value;
    };

    template <typename E> class expected<void, E> final {
    public:
        using value_type = void;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        template <typename U> using rebind = expected<U, E>;

        constexpr expected() noexcept : m_has_value(true) {}

        constexpr expected(const expected& other) noexcept(dh::is_nothrow_copy_constructible_v<E>)
            requires dh::is_copy_constructible_v<E>
            : m_has_value(other.m_has_value) {
            if (!m_has_value) {
                ::new (static_cast<void*>(error_ptr())) E(*other.error_ptr());
            }
        }

        constexpr expected(expected&& other) noexcept(dh::is_nothrow_move_constructible_v<E>)
            requires dh::is_move_constructible_v<E>
            : m_has_value(other.m_has_value) {
            if (!m_has_value) {
                ::new (static_cast<void*>(error_ptr())) E(static_cast<E&&>(*other.error_ptr()));
            }
        }

        template <typename G, typename = dh::enable_if_t<dh::is_constructible_v<E, const G&>>>
        constexpr expected(const unexpected<G>& e) noexcept(dh::is_nothrow_constructible_v<E, const G&>) : m_has_value(false) {
            ::new (static_cast<void*>(error_ptr())) E(e.error());
        }

        template <typename G, typename = dh::enable_if_t<dh::is_constructible_v<E, G>>>
        constexpr expected(unexpected<G>&& e) noexcept(dh::is_nothrow_constructible_v<E, G>) : m_has_value(false) {
            ::new (static_cast<void*>(error_ptr())) E(static_cast<G&&>(e.error()));
        }

        constexpr explicit expected(in_place_t) noexcept : m_has_value(true) {}

        template <typename... Args, typename = dh::enable_if_t<dh::is_constructible_v<E, Args...>>>
        constexpr explicit expected(unexpect_t, Args&&... args) noexcept(dh::is_nothrow_constructible_v<E, Args...>) : m_has_value(false) {
            ::new (static_cast<void*>(error_ptr())) E(dh::forward<Args>(args)...);
        }

        ~expected() requires dh::is_trivially_destructible_v<E> = default;
        ~expected() requires(!dh::is_trivially_destructible_v<E>) { destroy_active(); }

        constexpr expected& operator=(const expected& other) noexcept(dh::is_nothrow_copy_constructible_v<E> && dh::is_nothrow_copy_assignable_v<E>)
            requires dh::is_copy_assignable_v<E> && dh::is_copy_constructible_v<E>
        {
            if (this == &other) {
                return *this;
            }
            if (m_has_value && other.m_has_value) {
            } else if (!m_has_value && !other.m_has_value) {
                *error_ptr() = *other.error_ptr();
            } else if (m_has_value) {
                ::new (static_cast<void*>(error_ptr())) E(*other.error_ptr());
                m_has_value = false;
            } else {
                error_ptr()->~E();
                m_has_value = true;
            }
            return *this;
        }

        constexpr expected& operator=(expected&& other) noexcept(dh::is_nothrow_move_constructible_v<E> && dh::is_nothrow_move_assignable_v<E>)
            requires dh::is_move_assignable_v<E> && dh::is_move_constructible_v<E>
        {
            if (this == &other) {
                return *this;
            }
            if (m_has_value && other.m_has_value) {
            } else if (!m_has_value && !other.m_has_value) {
                *error_ptr() = static_cast<E&&>(*other.error_ptr());
            } else if (m_has_value) {
                ::new (static_cast<void*>(error_ptr())) E(static_cast<E&&>(*other.error_ptr()));
                m_has_value = false;
            } else {
                error_ptr()->~E();
                m_has_value = true;
            }
            return *this;
        }

        template <typename G, typename = dh::enable_if_t<dh::is_constructible_v<E, const G&> && dh::is_assignable_v<E&, const G&>>> constexpr expected&
        operator=(const unexpected<G>& e) noexcept(dh::is_nothrow_constructible_v<E, const G&> && dh::is_nothrow_assignable_v<E&, const G&>) {
            if (m_has_value) {
                ::new (static_cast<void*>(error_ptr())) E(e.error());
                m_has_value = false;
            } else {
                *error_ptr() = e.error();
            }
            return *this;
        }

        template <typename G, typename = dh::enable_if_t<dh::is_constructible_v<E, G> && dh::is_assignable_v<E&, G>>>
        constexpr expected& operator=(unexpected<G>&& e) noexcept(dh::is_nothrow_constructible_v<E, G> && dh::is_nothrow_assignable_v<E&, G>) {
            if (m_has_value) {
                ::new (static_cast<void*>(error_ptr())) E(static_cast<G&&>(e.error()));
                m_has_value = false;
            } else {
                *error_ptr() = static_cast<G&&>(e.error());
            }
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept { return m_has_value; }

        constexpr explicit operator bool() const noexcept { return m_has_value; }

        constexpr void value() const& noexcept {
            if (!m_has_value) {
                detail::bad_access();
            }
        }

        constexpr void value() && noexcept {
            if (!m_has_value) {
                detail::bad_access();
            }
        }

        constexpr void operator*() const noexcept {}

        [[nodiscard]] constexpr E& error() & noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return *error_ptr();
        }

        [[nodiscard]] constexpr const E& error() const& noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return *error_ptr();
        }

        [[nodiscard]] constexpr E&& error() && noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return static_cast<E&&>(*error_ptr());
        }

        [[nodiscard]] constexpr const E&& error() const&& noexcept {
            if (m_has_value) {
                detail::bad_access();
            }
            return static_cast<const E&&>(*error_ptr());
        }

        constexpr void emplace() noexcept {
            destroy_active();
            m_has_value = true;
        }

        template <typename U, typename G> friend constexpr bool operator==(const expected& a, const expected<U, G>& b) noexcept
            requires dh::is_void_v<U>
        {
            if (a.has_value() != b.has_value()) {
                return false;
            }
            if (a.has_value()) {
                return true;
            }
            return a.error() == b.error();
        }

        template <typename G> friend constexpr bool operator==(const expected& a, const unexpected<G>& e) noexcept {
            return !a.has_value() && a.error() == e.error();
        }

    private:
        union storage_t {
            constexpr storage_t() noexcept : m_dummy() {}

            ~storage_t() noexcept requires dh::is_trivially_destructible_v<E> = default;
            ~storage_t() noexcept requires(!dh::is_trivially_destructible_v<E>) {}

            unsigned char m_dummy;
            E m_error;
        };

        constexpr E* error_ptr() noexcept { return dh::addressof(m_storage.m_error); }

        constexpr const E* error_ptr() const noexcept { return dh::addressof(m_storage.m_error); }

        constexpr void destroy_active() noexcept {
            if (!m_has_value) {
                if constexpr (!dh::is_trivially_destructible_v<E>) {
                    error_ptr()->~E();
                }
            }
        }

        storage_t m_storage;
        bool m_has_value;
    };

}
