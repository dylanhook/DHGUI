// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "slot_strip.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include <climits>
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        constexpr int k_gap = 6;

        constexpr float k_plus_bar = 2.0f;
        constexpr float k_plus_pad = 3.0f;

    }

    slot_square paint_slot_square(frame_ctx& ctx, rect square, bool on, float hover) noexcept {
        const auto& pal = ctx.theme().colors();
        ctx.pnt().filled_rect(square, pal.border_outer);
        const rect inside = square.inset(ctx.pnt().hairline());

        const math::color fill = on ? ctx.theme().accent_value() : math::lerp(pal.check_top, pal.control_top_hover, hover);
        ctx.pnt().filled_rect(inside, fill);
        return {.inside = inside, .ink = on ? pal.text_primary : pal.text_secondary};
    }

    slot_strip::slot_strip(reactive::signal<int>& selected, dh::size_t count, int first_value, dh::function<dh::optional<math::color>(dh::size_t)> fill) noexcept : m_selected(&selected), m_count(count < k_max_slots ? count : k_max_slots), m_first_value(first_value), m_fill(dh::move(fill)) {
        if (first_value > INT_MAX - static_cast<int>(m_count)) dh::fail_fast("slot selection out of range", FAST_FAIL_INVALID_ARG);
        for (auto& t : m_hover)
            t.snap(0.0f);
    }

    rect slot_strip::square_rect(const frame_ctx& ctx, dh::size_t index) const noexcept {
        const auto side = static_cast<float>(ctx.theme().metrics().toggle_box);
        const float y = m_rect.y + (m_rect.h - side) * 0.5f;
        return {m_rect.x + static_cast<float>(index) * (side + k_gap), y, side, side};
    }

    size slot_strip::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();

        if (m_count == 0) return {0, m.row_h};
        const int run = static_cast<int>(m_count) * m.toggle_box + static_cast<int>(m_count - 1) * k_gap;
        return {run, m.row_h};
    }

    void slot_strip::paint(frame_ctx& ctx) noexcept {
        if (!m_painted || ctx.frame_seq() != m_last_paint_seq + 1) {
            for (auto& t : m_hover)
                t.snap(0.0f);
        }
        m_painted = true;
        m_last_paint_seq = ctx.frame_seq();

        const int active = m_selected->peek_base();

        for (dh::size_t i = 0; i < m_count; ++i) {
            const bool on = m_first_value + static_cast<int>(i) == active;
            const auto held = m_fill ? m_fill(i) : dh::nullopt;
            const auto [inside, ink] = paint_slot_square(ctx, square_rect(ctx, i), on, m_hover[i].value());

            if (held) {
                ctx.pnt().filled_rect(inside.inset(k_plus_pad), *held);
            } else if (inside.w > 2.0f * k_plus_pad && inside.h > 2.0f * k_plus_pad) {
                ctx.pnt().filled_rect({inside.x + k_plus_pad, inside.y + (inside.h - k_plus_bar) * 0.5f, inside.w - 2.0f * k_plus_pad, k_plus_bar}, ink);
                ctx.pnt().filled_rect({inside.x + (inside.w - k_plus_bar) * 0.5f, inside.y + k_plus_pad, k_plus_bar, inside.h - 2.0f * k_plus_pad}, ink);
            }
        }
    }

    bool slot_strip::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const bool blocked = ctx.cursor_in_popup();
        bool consumed = false;

        for (dh::size_t i = 0; i < m_count; ++i) {
            const rect sq = square_rect(ctx, i);
            const bool inside = sq.contains(cursor);

            if (inside && !ctx.mouse_consumed() && !blocked) {
                ctx.focus().set_hovered(*this);
            }
            m_hover[i].set_target((inside && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

            if (inside && !blocked && !ctx.mouse_consumed() && ctx.input().key_pressed(VK_LBUTTON)) {
                ctx.consume_mouse();
                m_selected->set(m_first_value + static_cast<int>(i));
                consumed = true;
            }
        }
        return consumed;
    }

}
