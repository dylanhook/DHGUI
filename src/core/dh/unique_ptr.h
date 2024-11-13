// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include <new>
#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh {

    template <typename T> struct default_delete final {
        constexpr default_delete() noexcept = default;

        template <typename U>
            requires dh::is_convertible_v<U*, T*>
        constexpr default_delete(const default_delete<U>&) noexcept {}

        void operator()(T* p) const noexcept {
            if (p == nullptr) {
                return;
            }

            delete p;
        }
    };

    namespace detail {

        template <typename T, typename Deleter> class unique_ptr_storage final {
        public:
            constexpr unique_ptr_storage() noexcept = default;

            constexpr explicit unique_ptr_storage(T* p) noexcept : m_ptr(p) {}

            constexpr unique_ptr_storage(T* p, Deleter d) noexcept : m_ptr(p), m_deleter(dh::move(d)) {}

            unique_ptr_storage(const unique_ptr_storage&) = delete;
            unique_ptr_storage& operator=(const unique_ptr_storage&) = delete;
            unique_ptr_storage(unique_ptr_storage&&) = delete;
            unique_ptr_storage& operator=(unique_ptr_storage&&) = delete;
            ~unique_ptr_storage() = default;

            [[nodiscard]] T* ptr() const noexcept { return m_ptr; }

            void set_ptr(T* p) noexcept { m_ptr = p; }

            [[nodiscard]] Deleter& deleter() noexcept { return m_deleter; }

            [[nodiscard]] const Deleter& deleter() const noexcept { return m_deleter; }

        private:
            T* m_ptr{nullptr};
            [[msvc::no_unique_address]] Deleter m_deleter{};
        };

    }

    template <typename T, typename Deleter = default_delete<T>> class unique_ptr final {
    public:
        using pointer = T*;
        using element_type = T;
        using deleter_type = Deleter;

        constexpr unique_ptr() noexcept = default;

        constexpr unique_ptr(decltype(nullptr)) noexcept : unique_ptr() {}

        explicit unique_ptr(pointer p) noexcept : m_storage(p) {}

        unique_ptr(unique_ptr&& other) noexcept : m_storage(other.release(), dh::move(other.get_deleter())) {}

        template <typename U, typename D>
            requires dh::is_convertible_v<U*, T*> && dh::is_convertible_v<D, Deleter>
        unique_ptr(unique_ptr<U, D>&& other) noexcept : m_storage(static_cast<T*>(other.release()), Deleter{dh::move(other.get_deleter())}) {}

        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        ~unique_ptr() { reset(); }

        unique_ptr& operator=(unique_ptr&& other) noexcept {
            if (this != &other) {
                reset();
                m_storage.deleter() = dh::move(other.get_deleter());
                m_storage.set_ptr(other.release());
            }
            return *this;
        }

        template <typename U, typename D>
            requires dh::is_convertible_v<U*, T*> && dh::is_convertible_v<D, Deleter>
        unique_ptr& operator=(unique_ptr<U, D>&& other) noexcept {
            reset();
            m_storage.deleter() = Deleter{dh::move(other.get_deleter())};
            m_storage.set_ptr(static_cast<T*>(other.release()));
            return *this;
        }

        unique_ptr& operator=(decltype(nullptr)) noexcept {
            reset();
            return *this;
        }

        [[nodiscard]] pointer release() noexcept {
            pointer p = m_storage.ptr();
            m_storage.set_ptr(nullptr);
            return p;
        }

        void reset(pointer p = nullptr) noexcept {
            pointer old = m_storage.ptr();
            m_storage.set_ptr(p);
            if (old != nullptr) {
                m_storage.deleter()(old);
            }
        }

        void swap(unique_ptr& other) noexcept {
            dh::swap(m_storage.deleter(), other.m_storage.deleter());
            pointer tmp = m_storage.ptr();
            m_storage.set_ptr(other.m_storage.ptr());
            other.m_storage.set_ptr(tmp);
        }

        [[nodiscard]] pointer get() const noexcept { return m_storage.ptr(); }

        [[nodiscard]] deleter_type& get_deleter() noexcept { return m_storage.deleter(); }

        [[nodiscard]] const deleter_type& get_deleter() const noexcept { return m_storage.deleter(); }

        [[nodiscard]] explicit operator bool() const noexcept { return m_storage.ptr() != nullptr; }

        T& operator*() const noexcept { return *m_storage.ptr(); }

        T* operator->() const noexcept { return m_storage.ptr(); }

        friend bool operator==(const unique_ptr& a, decltype(nullptr)) noexcept { return a.m_storage.ptr() == nullptr; }

        friend bool operator==(const unique_ptr& a, const unique_ptr& b) noexcept { return a.m_storage.ptr() == b.m_storage.ptr(); }

    private:
        detail::unique_ptr_storage<T, Deleter> m_storage{};
    };

    template <typename T, typename... Args> [[nodiscard]] unique_ptr<T> make_unique(Args&&... args) noexcept {
        void* mem = dh::allocate_for<T>();
        if (mem == nullptr) {
            return unique_ptr<T>{};
        }
        T* obj = ::new (mem) T(dh::forward<Args>(args)...);
        return unique_ptr<T>{obj};
    }

}
