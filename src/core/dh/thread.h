// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

namespace dh {

    enum class stop_result : dh::u8 {
        requested,
        already_requested,
        no_control_block,
    };

    namespace detail::thread_impl {

        struct control_block;

        void release(control_block* block) noexcept;

        void retain(control_block* block) noexcept;

        [[nodiscard]] bool stop_requested(const control_block* block) noexcept;

        stop_result request_stop(control_block* block) noexcept;

        [[nodiscard]] control_block* make_control_block() noexcept;

    }

    class stop_token final {
    public:
        stop_token() noexcept = default;

        explicit stop_token(detail::thread_impl::control_block* block) noexcept;

        stop_token(const stop_token& other) noexcept;
        stop_token(stop_token&& other) noexcept;
        stop_token& operator=(const stop_token& other) noexcept;
        stop_token& operator=(stop_token&& other) noexcept;
        ~stop_token();

        [[nodiscard]] bool stop_requested() const noexcept { return detail::thread_impl::stop_requested(m_block); }

    private:
        detail::thread_impl::control_block* m_block{nullptr};
    };

    class stop_source final {
    public:
        stop_source() noexcept;

        stop_source(const stop_source& other) noexcept;
        stop_source(stop_source&& other) noexcept;
        stop_source& operator=(const stop_source& other) noexcept;
        stop_source& operator=(stop_source&& other) noexcept;
        ~stop_source();

        [[nodiscard]] stop_token get_token() const noexcept { return stop_token{m_block}; }

        [[nodiscard]] stop_result request_stop() noexcept { return detail::thread_impl::request_stop(m_block); }

        [[nodiscard]] bool stop_requested() const noexcept { return detail::thread_impl::stop_requested(m_block); }

    private:
        detail::thread_impl::control_block* m_block{nullptr};
    };

    namespace this_thread {

        void sleep_for_ms(dh::u64 ms) noexcept;

        void yield() noexcept;

        [[nodiscard]] dh::u64 get_id() noexcept;

    }

}
