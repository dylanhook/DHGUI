// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "panel_placement.h"

#include "ui/core/frame_ctx.h"
#include "ui/painter/painter.h"

#include <Windows.h>

namespace dh::ui::overlay {

    panel_placement::panel_placement(reactive::signal<float>& x, reactive::signal<float>& y, float margin) noexcept
        : m_x(&x), m_y(&y), m_margin(margin) {}

    rect panel_placement::place(const painter& pnt, size panel) noexcept {
        const float vp_w = static_cast<float>(pnt.viewport_w());
        const float vp_h = static_cast<float>(pnt.viewport_h());
        const rect wanted{m_x->peek() * vp_w, m_y->peek() * vp_h, static_cast<float>(panel.w), static_cast<float>(panel.h)};
        m_rect = clamp_to_viewport(wanted, vp_w, vp_h, m_margin);
        return m_rect;
    }

    void panel_placement::clear() noexcept {
        m_rect = rect{};
        m_dragging = false;
    }

    void panel_placement::handle_input(frame_ctx& ctx, menu_state menu) noexcept {
        if (menu == menu_state::closed) {
            m_dragging = false;
            return;
        }

        const float vp_w = static_cast<float>(ctx.pnt().viewport_w());
        const float vp_h = static_cast<float>(ctx.pnt().viewport_h());
        if (vp_w <= 0.0f || vp_h <= 0.0f) return;

        const point cursor = ctx.cursor();

        if (m_dragging) {
            if (!ctx.input().key_down(VK_LBUTTON)) {
                m_dragging = false;
                return;
            }

            const rect placed = clamp_to_viewport({cursor.x - m_drag_offset.x, cursor.y - m_drag_offset.y, m_rect.w, m_rect.h}, vp_w, vp_h, m_margin);
            m_x->set(placed.x / vp_w);
            m_y->set(placed.y / vp_h);
            ctx.consume_mouse();
            return;
        }

        if (!m_rect.contains(cursor)) return;
        if (!ctx.input().key_pressed(VK_LBUTTON) || ctx.mouse_consumed()) return;

        m_dragging = true;
        m_drag_offset = {cursor.x - m_rect.x, cursor.y - m_rect.y};
        ctx.consume_mouse();
    }

}
