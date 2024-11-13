// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "scroll_view.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/memory.h"

namespace dh::ui::controls {

    namespace {

        constexpr float k_scroll_gutter = 10.0f;

        constexpr float k_thumb_edge = 1.0f;

        constexpr float k_thumb_alpha_rest = 0.6f;
        constexpr float k_thumb_drag_lighten = 0.25f;

        constexpr float k_scroll_lock_grace = 0.45f;

        constexpr int k_measure_slack = 4;

        [[nodiscard]] math::color lighten(math::color c, float f) noexcept {
            c.r = static_cast<dh::u8>(c.r + (255 - c.r) * f);
            c.g = static_cast<dh::u8>(c.g + (255 - c.g) * f);
            c.b = static_cast<dh::u8>(c.b + (255 - c.b) * f);
            return c;
        }
    }

    scroll_view::scroll_view(int visible_h) noexcept : m_visible_h(visible_h), m_inner(adopt_child(dh::make_unique<layout::stack>(layout::orientation::vertical))) {
        if (!m_inner) dh::alloc_failed("ui::controls::scroll_view");
        m_inner->set_spacing(2);
        m_inner->set_cross_align(layout::align::stretch);
        m_thumb_hover.snap(0.0f);
    }

    size scroll_view::desired_size(const frame_ctx&) const noexcept {
        return {0, m_visible_h};
    }

    scroll_metrics scroll_view::bar_metrics(const frame_ctx& ctx) const noexcept {
        return {
            .window = m_rect.h,
            .total = static_cast<float>(m_content_h),
            .position = m_scroll,
            .track_start = m_rect.y,
            .track_length = m_rect.h,
            .thumb_min = static_cast<float>(ctx.theme().metrics().scroll_thumb_min),
        };
    }

    void scroll_view::measure(const frame_ctx& ctx, size available) noexcept {
        const size inner_box{available.w - static_cast<int>(k_scroll_gutter), available.h * k_measure_slack};
        m_inner->measure(ctx, inner_box);
        m_content_h = m_inner->desired_size(ctx).h;
    }

    void scroll_view::arrange(rect r) noexcept {
        m_rect = r;
    }

    void scroll_view::clamp_scroll() noexcept {
        const scroll_metrics extent{.window = m_rect.h, .total = static_cast<float>(m_content_h)};
        m_scroll = extent.clamped(m_scroll);
    }

    void scroll_view::paint(frame_ctx& ctx) noexcept {
        clamp_scroll();
        m_inner->arrange({m_rect.x, m_rect.y - m_scroll, m_rect.w - k_scroll_gutter, static_cast<float>(m_content_h)});
        {
            const painter::clip_scope clip{ctx.pnt(), m_rect};
            m_inner->paint(ctx);
        }

        const scroll_metrics bar = bar_metrics(ctx);
        if (bar.overflows()) {
            const float hov = m_thumb_hover.value();
            math::color thumb = ctx.theme().accent_value();
            if (m_dragging) thumb = lighten(thumb, k_thumb_drag_lighten);
            const float alpha = k_thumb_alpha_rest + (1.0f - k_thumb_alpha_rest) * hov;
            thumb.a = static_cast<dh::u8>(255.0f * alpha);

            const float bar_w = static_cast<float>(ctx.theme().metrics().scroll_bar_w);
            const float bar_x = m_rect.right() - bar_w - k_thumb_edge;
            ctx.pnt().filled_capsule_v({bar_x, bar.thumb_start(), bar_w, bar.thumb_length()}, thumb);
        }
    }

    bool scroll_view::on_input(frame_ctx& ctx) noexcept {
        bool consumed = false;
        const point cursor = ctx.cursor();
        const bool inside = m_rect.contains(cursor);
        const bool popup_blocks_scroll = ctx.popup_blocks_scroll();

        const bool blocked = ctx.cursor_in_popup();

        const scroll_metrics bar = bar_metrics(ctx);
        const bool has_overflow = bar.overflows();

        if (m_scroll_lock > 0.0f) {
            m_scroll_lock = dh::max(0.0f, m_scroll_lock - ctx.frame_dt());
        }
        if (inside && !blocked && has_overflow && m_scroll_lock > 0.0f && ctx.input().mouse_wheel_delta() != 0.0f) {
            ctx.lock_wheel();
        }
        if (inside && popup_blocks_scroll && ctx.input().mouse_wheel_delta() != 0.0f) {
            ctx.lock_wheel();
        }

        const bool any_captured = ctx.focus().any_captured();
        const bool press = ctx.input().mouse_pressed();
        const bool outside = !inside && !any_captured;
        if (inside || any_captured || !press) {
            const scoped_hover_restore hover{ctx.focus(), outside};
            consumed |= m_inner->on_input(ctx);
        }

        if (inside && !blocked && !popup_blocks_scroll && !ctx.mouse_consumed()) {
            const float w = ctx.input().mouse_wheel_delta();
            if (w != 0.0f) {
                m_scroll -= w * static_cast<float>(ctx.theme().metrics().row_h);
                clamp_scroll();

                m_scroll_lock = k_scroll_lock_grace;
                return true;
            }
        }

        if (has_overflow) {
            const rect lane{m_rect.right() - k_scroll_gutter, m_rect.y, k_scroll_gutter, m_rect.h};

            const bool over_thumb = !blocked && lane.contains(cursor) && bar.thumb_holds(cursor.y);
            m_thumb_hover.set_target((over_thumb || m_dragging) ? 1.0f : 0.0f, ctx.theme().times().hover);

            if (ctx.focus().is_captured(id())) {
                if (!ctx.input().key_down(VK_LBUTTON)) {
                    ctx.focus().release_capture();
                    m_dragging = false;
                } else {
                    m_scroll = bar.position_for_thumb_start(cursor.y - m_drag_offset);
                    ctx.consume_mouse();
                    return true;
                }
            }
            if (lane.contains(cursor) && !blocked && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                m_drag_offset = over_thumb ? cursor.y - bar.thumb_start() : bar.thumb_length() * 0.5f;
                ctx.focus().capture(*this);
                m_dragging = true;
                ctx.consume_mouse();
                return true;
            }
        }
        return consumed;
    }

}
