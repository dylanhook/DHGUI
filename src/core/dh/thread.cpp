// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "thread.h"

#include "core/dh/atomic.h"

#include <new>

namespace dh {

    namespace detail::thread_impl {

        struct control_block {
            dh::atomic<dh::i32> refcount{1};
            dh::atomic<dh::i32> stopped{0};
        };

        control_block* make_control_block() noexcept {
            void* mem = dh::allocate(sizeof(control_block));
            if (mem == nullptr) {
                dh::alloc_failed("dh::stop_source");
            }
            return ::new (mem) control_block{};
        }

        void retain(control_block* block) noexcept {
            if (block == nullptr) return;
            (void)block->refcount.fetch_add(1, dh::memory_order_relaxed);
        }

        void release(control_block* block) noexcept {
            if (block == nullptr) return;

            if (block->refcount.fetch_sub(1, dh::memory_order_acq_rel) == 1) {
                block->~control_block();
                dh::deallocate(block);
            }
        }

        bool stop_requested(const control_block* block) noexcept {
            if (block == nullptr) return false;
            return block->stopped.load(dh::memory_order_acquire) != 0;
        }

        stop_result request_stop(control_block* block) noexcept {
            if (block == nullptr) return stop_result::no_control_block;
            dh::i32 expected = 0;
            return block->stopped.compare_exchange_strong(expected, 1, dh::memory_order_acq_rel) ? stop_result::requested : stop_result::already_requested;
        }

    }

    stop_token::stop_token(detail::thread_impl::control_block* block) noexcept : m_block(block) {
        detail::thread_impl::retain(m_block);
    }

    stop_token::stop_token(const stop_token& other) noexcept : m_block(other.m_block) {
        detail::thread_impl::retain(m_block);
    }

    stop_token::stop_token(stop_token&& other) noexcept : m_block(other.m_block) {
        other.m_block = nullptr;
    }

    stop_token& stop_token::operator=(const stop_token& other) noexcept {
        if (this == &other) return *this;

        detail::thread_impl::retain(other.m_block);
        detail::thread_impl::release(m_block);
        m_block = other.m_block;
        return *this;
    }

    stop_token& stop_token::operator=(stop_token&& other) noexcept {
        if (this == &other) return *this;
        detail::thread_impl::release(m_block);
        m_block = other.m_block;
        other.m_block = nullptr;
        return *this;
    }

    stop_token::~stop_token() {
        detail::thread_impl::release(m_block);
    }

    stop_source::stop_source() noexcept : m_block(detail::thread_impl::make_control_block()) {}

    stop_source::stop_source(const stop_source& other) noexcept : m_block(other.m_block) {
        detail::thread_impl::retain(m_block);
    }

    stop_source::stop_source(stop_source&& other) noexcept : m_block(other.m_block) {
        other.m_block = nullptr;
    }

    stop_source& stop_source::operator=(const stop_source& other) noexcept {
        if (this == &other) return *this;
        detail::thread_impl::retain(other.m_block);
        detail::thread_impl::release(m_block);
        m_block = other.m_block;
        return *this;
    }

    stop_source& stop_source::operator=(stop_source&& other) noexcept {
        if (this == &other) return *this;
        detail::thread_impl::release(m_block);
        m_block = other.m_block;
        other.m_block = nullptr;
        return *this;
    }

    stop_source::~stop_source() {
        detail::thread_impl::release(m_block);
    }

    namespace this_thread {

        void sleep_for_ms(dh::u64 ms) noexcept {
            constexpr dh::u64 k_max_sleep_ms = 0xFFFFFFFEu;
            if (ms > k_max_sleep_ms) {
                dh::fail_fast("sleep_for_ms exceeds the DWORD range", FAST_FAIL_INVALID_ARG);
            }
            ::Sleep(static_cast<DWORD>(ms));
        }

        void yield() noexcept {
            (void)::SwitchToThread();
        }

        dh::u64 get_id() noexcept {
            return static_cast<dh::u64>(::GetCurrentThreadId());
        }

    }

}
