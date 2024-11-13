// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include <new>
#include "core/dh/atomic.h"
#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh {

    namespace detail {

        struct shared_ctrl_block final {
            dh::atomic<dh::i32> m_strong{1};
            void (*m_dispose)(shared_ctrl_block*) noexcept {nullptr};

            shared_ctrl_block() noexcept = default;

            shared_ctrl_block(const shared_ctrl_block&) = delete;
            shared_ctrl_block& operator=(const shared_ctrl_block&) = delete;
            shared_ctrl_block(shared_ctrl_block&&) = delete;
            shared_ctrl_block& operator=(shared_ctrl_block&&) = delete;
            ~shared_ctrl_block() = default;
        };

        template <typename T> struct alignas(alignof(T) > alignof(shared_ctrl_block) ? alignof(T) : alignof(shared_ctrl_block)) shared_ctrl_combined final {
            shared_ctrl_block m_base{};
            alignas(T) unsigned char m_storage[sizeof(T)]{};

            shared_ctrl_combined(const shared_ctrl_combined&) = delete;
            shared_ctrl_combined& operator=(const shared_ctrl_combined&) = delete;
            shared_ctrl_combined(shared_ctrl_combined&&) = delete;
            shared_ctrl_combined& operator=(shared_ctrl_combined&&) = delete;

            shared_ctrl_combined() noexcept { m_base.m_dispose = &dispose_thunk; }

            ~shared_ctrl_combined() = default;

            [[nodiscard]] T* object() noexcept { return reinterpret_cast<T*>(&m_storage[0]); }

            static void dispose_thunk(shared_ctrl_block* cb) noexcept {
                auto* self = reinterpret_cast<shared_ctrl_combined*>(cb);
                self->object()->~T();
                dh::deallocate(self);
            }
        };

        inline void shared_acquire(shared_ctrl_block* cb) noexcept {
            if (cb != nullptr) {
                (void)cb->m_strong.fetch_add(1, dh::memory_order_relaxed);
            }
        }

        inline void shared_release(shared_ctrl_block* cb) noexcept {
            if (cb == nullptr) {
                return;
            }
            if (cb->m_strong.fetch_sub(1, dh::memory_order_acq_rel) == 1) {
                cb->m_dispose(cb);
            }
        }

    }

    template <typename T> class shared_ptr final {
    public:
        using element_type = T;

        constexpr shared_ptr() noexcept = default;

        constexpr shared_ptr(decltype(nullptr)) noexcept : shared_ptr() {}

        shared_ptr(const shared_ptr& other) noexcept : m_obj(other.m_obj), m_ctrl(other.m_ctrl) { detail::shared_acquire(m_ctrl); }

        shared_ptr(shared_ptr&& other) noexcept : m_obj(other.m_obj), m_ctrl(other.m_ctrl) {
            other.m_obj = nullptr;
            other.m_ctrl = nullptr;
        }

        template <typename U>
            requires dh::is_convertible_v<U*, T*>
        shared_ptr(const shared_ptr<U>& other) noexcept : m_obj(static_cast<T*>(other.m_obj)), m_ctrl(other.m_ctrl) {
            detail::shared_acquire(m_ctrl);
        }

        ~shared_ptr() { detail::shared_release(m_ctrl); }

        shared_ptr& operator=(const shared_ptr& other) noexcept {
            if (this != &other) {
                shared_ptr tmp{other};
                swap(tmp);
            }
            return *this;
        }

        shared_ptr& operator=(shared_ptr&& other) noexcept {
            if (this != &other) {
                shared_ptr tmp{static_cast<shared_ptr&&>(other)};
                swap(tmp);
            }
            return *this;
        }

        shared_ptr& operator=(decltype(nullptr)) noexcept {
            reset();
            return *this;
        }

        void reset() noexcept {
            shared_ptr tmp{};
            swap(tmp);
        }

        void swap(shared_ptr& other) noexcept {
            T* tp = m_obj;
            detail::shared_ctrl_block* tc = m_ctrl;
            m_obj = other.m_obj;
            m_ctrl = other.m_ctrl;
            other.m_obj = tp;
            other.m_ctrl = tc;
        }

        [[nodiscard]] T* get() const noexcept { return m_obj; }

        [[nodiscard]] dh::i32 use_count() const noexcept {
            return m_ctrl == nullptr ? 0 : m_ctrl->m_strong.load(dh::memory_order_acquire);
        }

        [[nodiscard]] explicit operator bool() const noexcept { return m_obj != nullptr; }

        T& operator*() const noexcept { return *m_obj; }

        T* operator->() const noexcept { return m_obj; }

        friend bool operator==(const shared_ptr& a, const shared_ptr& b) noexcept { return a.m_obj == b.m_obj; }

        friend bool operator==(const shared_ptr& a, decltype(nullptr)) noexcept { return a.m_obj == nullptr; }

    private:
        template <typename U, typename... Args> friend shared_ptr<U> make_shared(Args&&... args) noexcept;

        template <typename U> friend class shared_ptr;

        T* m_obj{nullptr};
        detail::shared_ctrl_block* m_ctrl{nullptr};
    };

    template <typename T, typename... Args> [[nodiscard]] shared_ptr<T> make_shared(Args&&... args) noexcept {
        void* mem = dh::allocate_for<detail::shared_ctrl_combined<T>>();
        if (mem == nullptr) {
            return shared_ptr<T>{};
        }
        auto* block = ::new (mem) detail::shared_ctrl_combined<T>();
        T* obj = ::new (static_cast<void*>(block->object())) T(dh::forward<Args>(args)...);

        shared_ptr<T> out{};
        out.m_obj = obj;
        out.m_ctrl = &block->m_base;
        return out;
    }

}
