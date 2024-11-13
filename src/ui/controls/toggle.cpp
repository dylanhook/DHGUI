// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "toggle.h"

#include "ui/controls/color_picker.h"
#include "ui/controls/list_link.h"
#include "ui/controls/slider.h"
#include "ui/controls/slot_strip.h"
#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        constexpr int k_trailing_gap = 8;

    }

    void paint_toggle_box(frame_ctx& ctx, rect bounds, float checked, float hover) noexcept {
        const auto& colors = ctx.theme().colors();
        auto& pnt = ctx.pnt();
        pnt.filled_rect(bounds, colors.border_outer);
        const rect inside = bounds.inset(pnt.hairline());
        const auto top = math::lerp(colors.check_top, colors.check_top_hover, hover);
        pnt.vertical_gradient(inside, math::lerp(top, ctx.theme().accent_value(), checked), math::lerp(math::lerp(colors.check_bot, colors.check_bot_hover, hover), math::lerp(colors.check_bot, ctx.theme().accent_value(), ctx.theme().metrics().accent_fill_mix), checked));
    }

    toggle::toggle(dh::string label, reactive::signal<bool>& bound, const i_context_editor_factory* editors) noexcept : m_label(dh::move(label)), m_bound(&bound) {
        if (editors != nullptr) m_context_editor = editors->for_boolean(*this, m_label, bound);
        m_check_t.snap(bound.peek_base() ? 1.0f : 0.0f);
        m_hover_t.snap(0.0f);
    }

    toggle::~toggle() = default;

    toggle* toggle::with_context_editor(dh::unique_ptr<i_context_editor> editor) noexcept {
        m_context_editor = dh::move(editor);
        return this;
    }

    toggle* toggle::with_color(reactive::signal<math::color>& bound) noexcept {
        reactive::signal<math::color>* const one[]{&bound};
        return with_colors(dh::span<reactive::signal<math::color>* const>{one, 1});
    }

    toggle* toggle::with_colors(reactive::signal<math::color>& first, reactive::signal<math::color>& second) noexcept {
        reactive::signal<math::color>* const pair[]{&first, &second};
        return with_colors(dh::span<reactive::signal<math::color>* const>{pair, 2});
    }

    toggle* toggle::with_colors(dh::span<reactive::signal<math::color>* const> bound, dh::function<dh::size_t()> visible) noexcept {
        m_color_count = bound.size() < k_max_inline_colors ? bound.size() : k_max_inline_colors;
        for (dh::size_t i = 0; i < m_color_count; ++i) {
            m_colors[i] = adopt_child(dh::make_unique<color_picker>(dh::string{}, *bound[i]));
            if (!m_colors[i]) dh::alloc_failed("ui::controls::toggle swatch");
        }
        m_visible_colors = dh::move(visible);
        return this;
    }

    color_picker* toggle::color_at(dh::size_t index) noexcept {
        return index < m_color_count ? m_colors[index].get() : nullptr;
    }

    dh::size_t toggle::visible_colors() const noexcept {
        if (!m_visible_colors) return m_color_count;
        const dh::size_t want = m_visible_colors();
        return want < m_color_count ? want : m_color_count;
    }

    toggle* toggle::with_slots(reactive::signal<int>& selected, dh::size_t count, int first_value, dh::function<dh::optional<math::color>(dh::size_t)> fill) noexcept {
        m_slots = adopt_child(dh::make_unique<slot_strip>(selected, count, first_value, dh::move(fill)));
        if (!m_slots) dh::alloc_failed("ui::controls::toggle slots");
        return this;
    }

    toggle* toggle::with_list_link(dh::function<void()> open, dh::function<bool()> visible) noexcept {
        m_list_link = adopt_child(dh::make_unique<list_link>(dh::move(open)));
        if (!m_list_link) dh::alloc_failed("ui::controls::toggle list link");
        m_list_link_visible = dh::move(visible);
        return this;
    }

    bool toggle::list_link_visible() const noexcept {
        return m_list_link && m_bound->peek_base() && (!m_list_link_visible || m_list_link_visible());
    }

    toggle* toggle::with_slider_value(slider& value_slider) noexcept {
        m_value_slider = &value_slider;
        return this;
    }

    toggle* toggle::set_editor_name(dh::string display_name) noexcept {
        if (m_context_editor) m_context_editor->set_display_name(dh::move(display_name));
        return this;
    }

    toggle* toggle::with_status(dh::function<dh::string_view()> status) noexcept {
        m_status = dh::move(status);
        return this;
    }

    int toggle::trailing_reserved_w(const frame_ctx& ctx) const noexcept {
        int w = 0;

        if (m_value_slider && m_bound->peek_base()) {
            w += static_cast<int>(m_value_slider->inline_value_width(ctx) + 0.5f);
        }
        const dh::size_t shown = visible_colors();
        for (dh::size_t i = 0; i < shown; ++i) {
            if (w > 0) w += k_trailing_gap;
            w += m_colors[i]->desired_size(ctx).w;
        }
        if (m_slots) {
            if (w > 0) w += k_trailing_gap;
            w += m_slots->desired_size(ctx).w;
        }
        if (list_link_visible()) {
            if (w > 0) w += k_trailing_gap;
            w += m_list_link->desired_size(ctx).w;
        }
        if (w > 0) w += k_trailing_gap;
        return w;
    }

    float toggle::value_right_edge(const frame_ctx& ctx) const noexcept {
        float x = m_rect.right();
        if (list_link_visible()) {
            x -= static_cast<float>(m_list_link->desired_size(ctx).w + k_trailing_gap);
        }
        if (m_slots) {
            x -= static_cast<float>(m_slots->desired_size(ctx).w + k_trailing_gap);
        }
        const dh::size_t shown = visible_colors();
        for (dh::size_t i = 0; i < shown; ++i) {
            x -= static_cast<float>(m_colors[i]->desired_size(ctx).w + k_trailing_gap);
        }
        return x;
    }

    rect toggle::slider_value_bounds(const frame_ctx& ctx) const noexcept {
        if (!m_value_slider) return {};
        const float width = m_value_slider->inline_value_width(ctx);
        return {value_right_edge(ctx) - width, m_rect.y, width, m_rect.h};
    }

    rect toggle::primary_rect(const frame_ctx& ctx) const noexcept {
        const float trail = static_cast<float>(trailing_reserved_w(ctx));
        return {m_rect.x, m_rect.y, m_rect.w - trail, m_rect.h};
    }

    void toggle::arrange_trailing(const frame_ctx& ctx) noexcept {
        const dh::size_t shown = visible_colors();
        const bool show_slots = m_slots != nullptr;
        const bool show_link = list_link_visible();
        if (m_list_link) m_list_link->set_visible(show_link ? visibility::shown : visibility::hidden);
        if (shown == 0 && !show_slots && !show_link) return;

        float cursor_x = m_rect.right();

        if (show_link) {
            const auto sz = m_list_link->desired_size(ctx);
            cursor_x -= static_cast<float>(sz.w);
            m_list_link->arrange({cursor_x, m_rect.y, static_cast<float>(sz.w), m_rect.h});
            cursor_x -= static_cast<float>(k_trailing_gap);
        }

        if (show_slots) {
            const auto sz = m_slots->desired_size(ctx);
            cursor_x -= static_cast<float>(sz.w);
            m_slots->arrange({cursor_x, m_rect.y, static_cast<float>(sz.w), m_rect.h});
            cursor_x -= static_cast<float>(k_trailing_gap);
        }
        for (dh::size_t i = shown; i-- > 0;) {
            const auto sz = m_colors[i]->desired_size(ctx);
            cursor_x -= static_cast<float>(sz.w);
            m_colors[i]->arrange({cursor_x, m_rect.y, static_cast<float>(sz.w), m_rect.h});
            if (i != 0) cursor_x -= static_cast<float>(k_trailing_gap);
        }
    }

    size toggle::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        const float text_w = ctx.pnt().measure_text_w(dh::string_view{m_label.data(), m_label.size()});
        int w = m.toggle_box + m.gap_normal + static_cast<int>(text_w + 0.5f);
        w += trailing_reserved_w(ctx);
        return {w, m.row_h};
    }

    void toggle::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();

        arrange_trailing(ctx);

        {
            const float target = m_bound->peek_base() ? 1.0f : 0.0f;
            // snap after a hidden frame so the old value doesnt flash.
            const bool continuous = m_painted && ctx.frame_seq() == m_last_paint_seq + 1;
            if (continuous) {
                m_check_t.set_target(target, ctx.theme().times().hover);
            } else {
                m_check_t.snap(target);
            }
            m_painted = true;
            m_last_paint_seq = ctx.frame_seq();
        }

        const rect prim = primary_rect(ctx);

        const float box_y = prim.y + (prim.h - static_cast<float>(m.toggle_box)) * 0.5f;
        const rect box{prim.x, box_y, static_cast<float>(m.toggle_box), static_cast<float>(m.toggle_box)};

        const float chk = m_check_t.value();
        const float h = m_hover_t.value();

        paint_toggle_box(ctx, box, chk, h);

        const float tx = box.right() + static_cast<float>(m.gap_normal) + 2.0f;
        const float ty = ctx.pnt().text_center_y(prim);
        const math::color tc = math::lerp(pal.text_label, pal.text_primary, chk);
        const dh::string_view label{m_label.data(), m_label.size()};
        ctx.pnt().text_truncated({tx, ty}, label, prim.right() - tx, tc);

        if (m_status && m_bound->peek_base()) {
            const dh::string_view note = m_status();
            if (!note.empty()) {
                const float nx = tx + ctx.pnt().measure_text_w(label) + static_cast<float>(m.gap_normal);
                if (nx < prim.right()) ctx.pnt().text_truncated({nx, ty}, note, prim.right() - nx, pal.text_muted);
            }
        }

        if (m_value_slider && m_bound->peek_base()) {
            m_value_slider->paint_inline_value(ctx, slider_value_bounds(ctx));
        }

        const dh::size_t shown = visible_colors();
        for (dh::size_t i = 0; i < shown; ++i)
            m_colors[i]->paint(ctx);
        if (m_slots) m_slots->paint(ctx);
        if (list_link_visible()) m_list_link->paint(ctx);
    }

    bool toggle::on_input(frame_ctx& ctx) noexcept {
        arrange_trailing(ctx);

        if (m_value_slider && !m_bound->peek_base()) {
            m_value_slider->finish_inline_edit(ctx);
        }

        bool consumed = false;
        if (list_link_visible()) consumed |= m_list_link->on_input(ctx);
        if (m_slots) consumed |= m_slots->on_input(ctx);

        const dh::size_t shown = visible_colors();
        for (dh::size_t i = 0; i < shown; ++i)
            consumed |= m_colors[i]->on_input(ctx);
        if (consumed) return true;

        if (m_context_editor && m_context_editor->open_on_right_click(ctx, m_rect)) {
            return true;
        }

        if (m_value_slider && m_bound->peek_base() && m_value_slider->on_inline_value_input(ctx, slider_value_bounds(ctx))) {
            return true;
        }

        const point cursor = ctx.cursor();
        const rect prim = primary_rect(ctx);
        const bool inside = prim.contains(cursor);

        const bool blocked = ctx.cursor_in_popup();

        if (inside && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }

        m_hover_t.set_target((inside && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (inside && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            m_bound->set(!m_bound->peek_base());
            ctx.consume_mouse();
            return true;
        }
        return false;
    }

}
