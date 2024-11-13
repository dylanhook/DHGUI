// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/unique_ptr.h"
#include "ui/animation/tween.h"
#include "ui/controls/scroll_geometry.h"
#include "ui/core/control.h"
#include "ui/layout/stack.h"

namespace dh::ui::controls {

    class hscroll_view final : public control {
    public:
        explicit hscroll_view(int visible_w) noexcept;
        ~hscroll_view() override = default;

        hscroll_view(const hscroll_view&) = delete;
        hscroll_view& operator=(const hscroll_view&) = delete;
        hscroll_view(hscroll_view&&) = delete;
        hscroll_view& operator=(hscroll_view&&) = delete;

        [[nodiscard]] layout::stack& content() noexcept { return *m_inner; }

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void measure(const frame_ctx& ctx, size available) noexcept override;
        void arrange(rect r) noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] scroll_metrics bar_metrics(const frame_ctx& ctx) const noexcept;
        void clamp_scroll() noexcept;

        [[nodiscard]] bool bar_input(frame_ctx& ctx, point cursor, bool blocked) noexcept;

        int m_visible_w;
        float m_scroll{0.0f};
        int m_content_w{0};

        float m_scroll_lock{0.0f};

        bool m_dragging{false};
        float m_drag_offset{0.0f};

        animation::tween m_thumb_hover;

        dh::unique_ptr<layout::stack> m_inner;
    };

}
