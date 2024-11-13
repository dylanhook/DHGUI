// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "hscroll_view.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"

namespace dh::ui::controls {

    namespace {

        constexpr float k_scroll_lane_h = 8.0f;

        constexpr float k_thumb_h = 2.0f;

        constexpr float k_thumb_edge = 1.0f;

        constexpr float k_thumb_alpha_rest = 0.6f;

        constexpr float k_scroll_lock_grace = 0.45f;

        constexpr float k_cap_min_radius = 1.5f;

        constexpr int k_measure_slack = 4;

        void filled_capsule_h(painter& pnt, rect r, math::color c) noexcept {
            if (r.w <= 0.0f || r.h <= 0.0f) return;
            const float radius = dh::min(r.w * 0.5f, r.h * 0.5f);
            if (radius < k_cap_min_radius) {
                pnt.filled_rect(r, c);
                return;
            }
            pnt.filled_rect({r.x + radius, r.y, r.w - radius * 2.0f, r.h}, c);
            const int cols = static_cast<int>(radius);
            for (int i = 0; i < cols; ++i) {
                const float dx = radius - (static_cast<float>(i) + 0.5f);
                const float half = dh::sqrt(dh::max(0.0f, radius * radius - dx * dx));
                const float h = half * 2.0f;
                const float y = r.y + (r.h - h) * 0.5f;
                pnt.filled_rect({r.x + static_cast<float>(i), y, 1.0f, h}, c);
                pnt.filled_rect({r.right() - static_cast<float>(i) - 1.0f, y, 1.0f, h}, c);
            }
        }
    }

    hscroll_view::hscroll_view(int visible_w) noexcept : m_visible_w(visible_w), m_inner(adopt_child(dh::make_unique<layout::stack>(layout::orientation::horizontal))) {
        if (!m_inner) dh::alloc_failed("ui::controls::hscroll_view");
        m_inner->set_spacing(2);
        m_inner->set_cross_align(layout::align::stretch);
        m_thumb_hover.snap(0.0f);
    }

    scroll_metrics hscroll_view::bar_metrics(const frame_ctx& ctx) const noexcept {
        return {
            .window = m_rect.w,
            .total = static_cast<float>(m_content_w),
            .position = m_scroll,
            .track_start = m_rect.x,
            .track_length = m_rect.w,
            .thumb_min = static_cast<float>(ctx.theme().metrics().scroll_thumb_min),
        };
    }

    size hscroll_view::desired_size(const frame_ctx& ctx) const noexcept {
        return {m_visible_w, m_inner->desired_size(ctx).h + ctx.theme().metrics().hscroll_bottom_padding};
    }

    void hscroll_view::measure(const frame_ctx& ctx, size available) noexcept {
        const size inner_box{available.w * k_measure_slack, dh::max(0, available.h - ctx.theme().metrics().hscroll_bottom_padding)};
        m_inner->measure(ctx, inner_box);
        m_content_w = m_inner->desired_size(ctx).w;
    }

    void hscroll_view::arrange(rect r) noexcept {
        m_rect = r;
    }

    void hscroll_view::clamp_scroll() noexcept {
        const scroll_metrics extent{.window = m_rect.w, .total = static_cast<float>(m_content_w)};
        m_scroll = extent.clamped(m_scroll);
    }

    void hscroll_view::paint(frame_ctx& ctx) noexcept {
        clamp_scroll();
        const rect content{m_rect.x, m_rect.y, m_rect.w,
            dh::max(0.0f, m_rect.h - static_cast<float>(ctx.theme().metrics().hscroll_bottom_padding))};
        m_inner->arrange({content.x - m_scroll, content.y, static_cast<float>(m_content_w), content.h});
        {
            const painter::clip_scope clip{ctx.pnt(), content};
            m_inner->paint(ctx);
        }

        const scroll_metrics bar = bar_metrics(ctx);
        if (bar.overflows()) {
            const auto& pal = ctx.theme().colors();

            const float hov = m_thumb_hover.value();
            math::color thumb = m_dragging ? pal.text_secondary : pal.text_muted;
            const float alpha = k_thumb_alpha_rest + (1.0f - k_thumb_alpha_rest) * hov;
            thumb.a = static_cast<dh::u8>(static_cast<float>(thumb.a) * alpha);

            const float bar_y = m_rect.bottom() - k_thumb_h - k_thumb_edge;
            filled_capsule_h(ctx.pnt(), {bar.thumb_start(), bar_y, bar.thumb_length(), k_thumb_h}, thumb);
        }
    }

    bool hscroll_view::bar_input(frame_ctx& ctx, point cursor, bool blocked) noexcept {
        const rect lane{m_rect.x, m_rect.bottom() - k_scroll_lane_h, m_rect.w, k_scroll_lane_h};
        const scroll_metrics bar = bar_metrics(ctx);

        const bool over_thumb = !blocked && lane.contains(cursor) && bar.thumb_holds(cursor.x);
        m_thumb_hover.set_target((over_thumb || m_dragging) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (ctx.focus().is_captured(id())) {
            if (!ctx.input().key_down(VK_LBUTTON)) {
                ctx.focus().release_capture();
                m_dragging = false;
            } else {
                m_scroll = bar.position_for_thumb_start(cursor.x - m_drag_offset);
                ctx.consume_mouse();
                return true;
            }
        }
        if (lane.contains(cursor) && !blocked && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            m_drag_offset = over_thumb ? cursor.x - bar.thumb_start() : bar.thumb_length() * 0.5f;
            ctx.focus().capture(*this);
            m_dragging = true;
            ctx.consume_mouse();
            return true;
        }
        return false;
    }

    bool hscroll_view::on_input(frame_ctx& ctx) noexcept {
        bool consumed = false;
        const point cursor = ctx.cursor();
        const bool inside = m_rect.contains(cursor);
        const bool popup_blocks_scroll = ctx.popup_blocks_scroll();

        const bool blocked = ctx.cursor_in_popup();

        const scroll_metrics bar = bar_metrics(ctx);
        const bool has_overflow = bar.overflows();

        const bool locked_by_other = ctx.wheel_locked();

        if (m_scroll_lock > 0.0f) {
            m_scroll_lock = dh::max(0.0f, m_scroll_lock - ctx.frame_dt());
        }
        if (inside && !blocked && has_overflow && m_scroll_lock > 0.0f && ctx.input().mouse_wheel_delta() != 0.0f) {
            ctx.lock_wheel();
        }
        if (inside && popup_blocks_scroll && ctx.input().mouse_wheel_delta() != 0.0f) {
            ctx.lock_wheel();
        }

        if (has_overflow && bar_input(ctx, cursor, blocked)) return true;

        const bool any_captured = ctx.focus().any_captured();
        const bool press = ctx.input().mouse_pressed();
        const bool outside = !inside && !any_captured;
        if (inside || any_captured || !press) {
            const scoped_hover_restore hover{ctx.focus(), outside};
            consumed |= m_inner->on_input(ctx);
        }

        if (inside && !blocked && !popup_blocks_scroll && has_overflow && !ctx.mouse_consumed() && !locked_by_other) {
            const float w = ctx.input().mouse_wheel_delta();
            const bool can_move = (w > 0.0f) ? (m_scroll > 0.0f) : (m_scroll < bar.span());
            if (w != 0.0f && can_move) {
                m_scroll -= w * static_cast<float>(ctx.theme().metrics().row_h);
                clamp_scroll();

                m_scroll_lock = k_scroll_lock_grace;
                ctx.consume_mouse();
                return true;
            }
        }

        return consumed;
    }

}
