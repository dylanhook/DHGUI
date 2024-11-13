// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "list_link.h"

#include "ui/controls/slot_strip.h"
#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/utility.h"

namespace dh::ui::controls {

    list_link::list_link(dh::function<void()> open) noexcept : m_open(dh::move(open)) {
        m_hover.snap(0.0f);
    }

    size list_link::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        return {m.toggle_box, m.row_h};
    }

    rect list_link::square_rect(const frame_ctx& ctx) const noexcept {
        const auto side = static_cast<float>(ctx.theme().metrics().toggle_box);
        return {m_rect.right() - side, m_rect.y + (m_rect.h - side) * 0.5f, side, side};
    }

    void list_link::paint(frame_ctx& ctx) noexcept {
        if (!m_painted || ctx.frame_seq() != m_last_paint_seq + 1) m_hover.snap(0.0f);
        m_painted = true;
        m_last_paint_seq = ctx.frame_seq();

        const auto [inside, ink] = paint_slot_square(ctx, square_rect(ctx), false, m_hover.value());
        ctx.pnt().chevron_right(inside.center(), ctx.theme().metrics().chevron_size, ink, ctx.theme().colors().text_shadow);
    }

    bool list_link::on_input(frame_ctx& ctx) noexcept {
        const bool inside = square_rect(ctx).contains(ctx.cursor()) && !ctx.cursor_in_popup();
        if (inside && !ctx.mouse_consumed()) ctx.focus().set_hovered(*this);
        m_hover.set_target(inside ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (!inside || ctx.mouse_consumed() || !ctx.input().key_pressed(VK_LBUTTON)) return false;
        ctx.consume_mouse();
        if (m_open) m_open();
        return true;
    }

}
