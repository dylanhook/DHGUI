// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/unique_ptr.h"
#include "ui/animation/tween.h"
#include "ui/controls/scroll_geometry.h"
#include "ui/core/control.h"
#include "ui/layout/stack.h"

namespace dh::ui::controls {

    class scroll_view final : public control {
    public:
        explicit scroll_view(int visible_h) noexcept;
        ~scroll_view() override = default;

        scroll_view(const scroll_view&) = delete;
        scroll_view& operator=(const scroll_view&) = delete;
        scroll_view(scroll_view&&) = delete;
        scroll_view& operator=(scroll_view&&) = delete;

        [[nodiscard]] layout::stack& content() noexcept { return *m_inner; }

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void measure(const frame_ctx& ctx, size available) noexcept override;
        void arrange(rect r) noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] scroll_metrics bar_metrics(const frame_ctx& ctx) const noexcept;
        void clamp_scroll() noexcept;

        int m_visible_h;
        float m_scroll{0.0f};
        int m_content_h{0};

        float m_scroll_lock{0.0f};

        bool m_dragging{false};
        float m_drag_offset{0.0f};

        animation::tween m_thumb_hover;

        dh::unique_ptr<layout::stack> m_inner;
    };

}
