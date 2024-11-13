// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "input/input_state.h"
#include "ui/core/types.h"

#include "core/dh/memory.h"

namespace dh::ui {

    class painter;
    class popup_layer;
    class focus_manager;

    namespace theme {
        class theme;
    }

    class frame_ctx final {
    public:
        frame_ctx(painter& pnt, const input::input_state& input, focus_manager& focus, popup_layer& popups, const theme::theme& theme_ref, float frame_dt, dh::u64 frame_seq) noexcept : m_painter(&pnt), m_input(&input), m_focus(&focus), m_popups(&popups), m_theme(&theme_ref), m_frame_dt(frame_dt), m_frame_seq(frame_seq) {}

        ~frame_ctx() = default;

        frame_ctx(const frame_ctx&) = delete;
        frame_ctx& operator=(const frame_ctx&) = delete;
        frame_ctx(frame_ctx&&) = delete;
        frame_ctx& operator=(frame_ctx&&) = delete;

        [[nodiscard]] painter& pnt() const noexcept { return *m_painter; }

        [[nodiscard]] const input::input_state& input() const noexcept { return *m_input; }

        [[nodiscard]] focus_manager& focus() const noexcept { return *m_focus; }

        [[nodiscard]] popup_layer& popups() const noexcept { return *m_popups; }

        [[nodiscard]] const theme::theme& theme() const noexcept { return *m_theme; }

        [[nodiscard]] float frame_dt() const noexcept { return m_frame_dt; }

        [[nodiscard]] dh::u64 frame_seq() const noexcept { return m_frame_seq; }

        [[nodiscard]] point cursor() const noexcept;

        [[nodiscard]] bool mouse_consumed() const noexcept { return m_mouse_consumed; }

        void consume_mouse() noexcept { m_mouse_consumed = true; }

        [[nodiscard]] bool cursor_in_popup() const noexcept;

        [[nodiscard]] bool popup_blocks_scroll() const noexcept;

        [[nodiscard]] bool wheel_locked() const noexcept { return m_wheel_locked; }

        void lock_wheel() noexcept { m_wheel_locked = true; }

        [[nodiscard]] bool keyboard_consumed() const noexcept { return m_keyboard_consumed; }

        void consume_keyboard() noexcept { m_keyboard_consumed = true; }

    private:
        painter* m_painter;
        const input::input_state* m_input;
        focus_manager* m_focus;
        popup_layer* m_popups;
        const theme::theme* m_theme;
        float m_frame_dt;
        dh::u64 m_frame_seq;

        bool m_mouse_consumed{false};
        bool m_keyboard_consumed{false};
        bool m_wheel_locked{false};
    };

}
