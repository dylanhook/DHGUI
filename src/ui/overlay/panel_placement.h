// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/algorithm.h"
#include "reactive/signal.h"
#include "ui/core/types.h"
#include "ui/overlay/overlay.h"

namespace dh::ui {
    class frame_ctx;
    class painter;
}

namespace dh::ui::overlay {

    [[nodiscard]] constexpr rect clamp_to_viewport(rect panel, float viewport_w, float viewport_h, float margin) noexcept {
        const float edge_x = dh::max(0.0f, viewport_w - panel.w);
        const float edge_y = dh::max(0.0f, viewport_h - panel.h);
        const float min_x = dh::min(margin, edge_x);
        const float min_y = dh::min(margin, edge_y);
        const float max_x = dh::max(min_x, edge_x - margin);
        const float max_y = dh::max(min_y, edge_y - margin);
        return {dh::clamp(panel.x, min_x, max_x), dh::clamp(panel.y, min_y, max_y), panel.w, panel.h};
    }

    class panel_placement final {
    public:
        panel_placement(reactive::signal<float>& x, reactive::signal<float>& y, float margin) noexcept;

        panel_placement(const panel_placement&) = delete;
        panel_placement& operator=(const panel_placement&) = delete;
        panel_placement(panel_placement&&) = delete;
        panel_placement& operator=(panel_placement&&) = delete;
        ~panel_placement() = default;

        [[nodiscard]] rect place(const painter& pnt, size panel) noexcept;

        void clear() noexcept;

        void handle_input(frame_ctx& ctx, menu_state menu) noexcept;

        [[nodiscard]] bool contains(point position) const noexcept { return m_rect.contains(position); }

        [[nodiscard]] rect bounds() const noexcept { return m_rect; }

    private:
        reactive::signal<float>* m_x;
        reactive::signal<float>* m_y;
        float m_margin;

        rect m_rect{};
        bool m_dragging{false};
        point m_drag_offset{};
    };

}
