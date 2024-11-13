// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "combobox.h"

#include "ui/controls/search_match.h"
#include "ui/controls/text_input.h"
#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    combobox::combobox(dh::string label, reactive::signal<int>& bound, dh::vector<dh::string> options, const i_context_editor_factory* editors) noexcept : m_label(dh::move(label)), m_bound(&bound), m_options(dh::move(options)) {
        if (editors != nullptr) {
            m_context_editor = editors->for_option_list(*this, m_label, bound, m_options);
            if (!m_context_editor) dh::alloc_failed("ui::controls::combobox");
        }
        for (dh::size_t i = 0; i < m_options.size(); ++i)
            m_filtered_indices.push_back(static_cast<int>(i));
        if (searchable()) {
            m_search_input = adopt_child(dh::make_unique<text_input>(dh::string{}, m_filter, 64));
            m_search_input->set_placeholder("search...");
        }
        m_hover_t.snap(0.0f);
    }

    combobox::~combobox() = default;

    size combobox::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        const int label_off = label_offset(ctx);
        return {0, label_off + m.control_h};
    }

    int combobox::label_offset(const frame_ctx& ctx) const noexcept {
        return m_label.empty() ? 0 : ctx.pnt().text_line_h() + ctx.theme().metrics().label_row_gap;
    }

    rect combobox::header_rect(const frame_ctx& ctx) const noexcept {
        return {m_rect.x, m_rect.y + static_cast<float>(label_offset(ctx)), m_rect.w, static_cast<float>(ctx.theme().metrics().control_h)};
    }

    bool combobox::searchable() const noexcept {
        return m_options.size() >= k_popup_search_min_options;
    }

    void combobox::refresh_filtered() noexcept {
        if (!searchable()) return;
        const dh::string& filter = m_filter.peek_base();
        if (filter == m_applied_filter) return;
        m_applied_filter = filter;
        m_filtered_indices.clear();
        const dh::string_view needle{filter.data(), filter.size()};
        for (dh::size_t i = 0; i < m_options.size(); ++i) {
            const auto& label = m_options[i];
            if (option_contains_ci(dh::string_view{label.data(), label.size()}, needle))
                m_filtered_indices.push_back(static_cast<int>(i));
        }
        m_first_visible = 0;
        m_popup_hover_idx = -1;
    }

    int combobox::option_at(int filtered_row) const noexcept {
        return filtered_row >= 0 && filtered_row < static_cast<int>(m_filtered_indices.size()) ?
            m_filtered_indices[static_cast<dh::size_t>(filtered_row)] : -1;
    }

    int combobox::visible_items() const noexcept {
        const int count = static_cast<int>(m_filtered_indices.size());
        return count < k_max_visible_items ? count : k_max_visible_items;
    }

    bool combobox::overflows() const noexcept {
        return static_cast<int>(m_filtered_indices.size()) > k_max_visible_items;
    }

    void combobox::clamp_scroll() noexcept {
        const int max_first = static_cast<int>(m_filtered_indices.size()) - visible_items();
        if (m_first_visible > max_first) m_first_visible = max_first;
        if (m_first_visible < 0) m_first_visible = 0;
    }

    popup_scrollbar combobox::scrollbar(const frame_ctx& ctx, rect popup) const noexcept {
        const auto& m = ctx.theme().metrics();
        return {.popup = popup,
            .pad = list_popup_pad(ctx),
            .bar_w = static_cast<float>(m.scroll_bar_w),
            .thumb_min = static_cast<float>(m.scroll_thumb_min),
            .visible = visible_items(),
            .total = static_cast<int>(m_filtered_indices.size()),
            .first_visible = m_first_visible};
    }

    rect combobox::list_bounds(const frame_ctx& ctx, rect popup) const noexcept {
        if (!searchable()) return popup;
        const float offset = static_cast<float>(ctx.theme().metrics().row_h + ctx.theme().metrics().gap_normal);
        return {popup.x, popup.y + offset, popup.w, popup.h - offset};
    }

    rect combobox::search_bounds(const frame_ctx& ctx, rect popup) const noexcept {
        return {popup.x + list_popup_pad(ctx), popup.y + list_popup_pad(ctx), popup.w - list_popup_pad(ctx) * 2.0f,
            static_cast<float>(ctx.theme().metrics().row_h)};
    }

    rect combobox::compute_popup_bounds(const frame_ctx& ctx) const noexcept {
        const rect header = header_rect(ctx);
        const int rows = visible_items() > 0 ? visible_items() : 1;
        const float search_h = searchable() ? static_cast<float>(ctx.theme().metrics().row_h + ctx.theme().metrics().gap_normal) : 0.0f;
        const float popup_h = list_popup_pad(ctx) * 2.0f + search_h + static_cast<float>(ctx.theme().metrics().popup_row_h) * static_cast<float>(rows);

        const float vp_h = static_cast<float>(ctx.pnt().viewport_h());

        float py = m_rect.bottom();
        if (py + popup_h > vp_h) {
            py = header.y - popup_h;
            if (py < 0.0f) py = 0.0f;
        }
        return {m_rect.x, py, m_rect.w, popup_h};
    }

    void combobox::open_popup(frame_ctx& ctx) noexcept {
        m_filter.set(dh::string{});
        refresh_filtered();

        m_first_visible = m_bound->peek_base() - visible_items() / 2;
        clamp_scroll();

        popup_record rec;
        rec.z_band = ctx.theme().metrics().popup_z_combobox;
        rec.bounds = compute_popup_bounds(ctx);
        rec.draw = [this](painter& pnt, frame_ctx& fctx) noexcept { paint_popup(pnt, fctx); };
        rec.input = [this](frame_ctx& fctx) noexcept { return input_popup(fctx); };
        rec.on_dismissed = [this, focus = &ctx.focus()]() noexcept {
            if (m_search_input && focus->is_focused(m_search_input->id())) focus->clear_focus();
        };
        m_popup.open(ctx, static_cast<popup_record&&>(rec));
        if (m_search_input) ctx.focus().set_text_focus(*m_search_input);
    }

    void combobox::paint_popup(painter& pnt, frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();
        const rect r = m_popup.bounds();
        const rect list = list_bounds(ctx, r);

        pnt.chiseled_panel(r, pal.popup_border_outer, pal.popup_border_inner, pal.popup_bg);
        if (m_search_input) {
            m_search_input->arrange(search_bounds(ctx, r));
            m_search_input->paint(ctx);
        }

        const popup_scrollbar sb = scrollbar(ctx, list);
        const float content_x = list.x + 1.0f;
        const float content_y = list.y + list_popup_pad(ctx);
        const float item_h = static_cast<float>(m.popup_row_h);

        const float content_w = list.w - 2.0f - (overflows() ? static_cast<float>(m.scroll_bar_w) + 2.0f : 0.0f);
        const float text_inset = static_cast<float>(m.popup_row_text_inset);

        const int shown = visible_items();
        if (shown == 0) {
            const rect empty_row{content_x, content_y, content_w, item_h};
            pnt.text({empty_row.x + text_inset, pnt.text_center_y(empty_row)}, "no matches", pal.text_muted);
        }
        for (int row = 0; row < shown; ++row) {
            const int i = option_at(m_first_visible + row);
            if (i < 0) break;
            const rect item_r{content_x, content_y + item_h * static_cast<float>(row), content_w, item_h};

            const bool sel = (m_bound->peek_base() == i);
            const bool hov = (m_popup_hover_idx == i);

            if (sel) {
                pnt.filled_rect(item_r, ctx.theme().accent_wash());
                pnt.filled_rect({item_r.x, item_r.y, static_cast<float>(m.row_stripe_w), item_r.h}, ctx.theme().accent_value());
            } else if (hov) {
                pnt.filled_rect(item_r, pal.popup_row_hover);
            }

            const float tx = item_r.x + text_inset;
            const float ty = pnt.text_center_y(item_r);
            const math::color tc = sel ? pal.text_primary : (hov ? pal.text_primary : pal.text_secondary);
            const dh::string_view opt_view{m_options[static_cast<dh::size_t>(i)].data(), m_options[static_cast<dh::size_t>(i)].size()};
            pnt.text_truncated({tx, ty}, opt_view, item_r.w - text_inset - static_cast<float>(m.text_truncate_margin), tc);
        }

        if (overflows()) {
            const rect track = sb.track();
            pnt.filled_rect(track, pal.form_dark);
            pnt.filled_rect({track.x + 1.0f, sb.bar().thumb_start(), track.w - 2.0f, sb.bar().thumb_length()}, m_dragging_thumb ? pal.text_secondary : pal.text_muted);
        }
    }

    bool combobox::input_popup(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();

        if (ctx.input().key_pressed(VK_ESCAPE)) {
            if (m_search_input && ctx.focus().is_focused(m_search_input->id())) ctx.focus().clear_focus();
            ctx.consume_keyboard();
            m_popup.close();
            return true;
        }
        if (m_search_input) {
            m_search_input->arrange(search_bounds(ctx, compute_popup_bounds(ctx)));
            (void)m_search_input->on_input(ctx);
            refresh_filtered();
        }
        m_popup.set_bounds(compute_popup_bounds(ctx));
        const rect bounds = m_popup.bounds();
        const rect list = list_bounds(ctx, bounds);
        const bool inside = bounds.contains(cursor);

        if (overflows()) {
            const popup_scrollbar sb = scrollbar(ctx, list);

            if (inside && !ctx.wheel_locked()) {
                const float w = ctx.input().mouse_wheel_delta();
                if (w != 0.0f) {
                    m_first_visible -= static_cast<int>(w);
                    clamp_scroll();
                    ctx.lock_wheel();
                }
            }

            if (ctx.input().key_pressed(VK_LBUTTON) && sb.track().contains(cursor)) {
                m_dragging_thumb = true;
            }
            if (m_dragging_thumb) {
                if (!ctx.input().key_down(VK_LBUTTON)) {
                    m_dragging_thumb = false;
                } else {
                    const scroll_metrics bar = sb.bar();
                    m_first_visible = bar.row_position_for_thumb_start(cursor.y - bar.thumb_length() * 0.5f);
                    clamp_scroll();
                    ctx.consume_mouse();
                    return true;
                }
            }
        }

        const int row = popup_item_at(list, list_popup_pad(ctx), static_cast<float>(ctx.theme().metrics().popup_row_h), visible_items(), cursor);
        m_popup_hover_idx = row >= 0 ? option_at(m_first_visible + row) : -1;

        if (ctx.input().key_pressed(VK_LBUTTON) && m_popup_hover_idx >= 0 && m_popup_hover_idx < static_cast<int>(m_options.size())) {
            m_bound->set(m_popup_hover_idx);
            ctx.consume_mouse();
            if (m_search_input && ctx.focus().is_focused(m_search_input->id())) ctx.focus().clear_focus();
            m_popup.close();
            return true;
        }
        return inside;
    }

    void combobox::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();

        const dh::string_view label_view{m_label.data(), m_label.size()};
        ctx.pnt().text_truncated({m_rect.x, m_rect.y}, label_view, m_rect.w, pal.text_label);

        const rect header = header_rect(ctx);

        const float t = m_hover_t.value();
        const math::color top = math::lerp(pal.control_top, pal.control_top_hover, t);
        const math::color bot = math::lerp(pal.control_bot, pal.control_bot_hover, t);

        ctx.pnt().filled_rect(header, pal.border_outer);
        const rect h_inside = header.inset(ctx.pnt().hairline());
        ctx.pnt().vertical_gradient(h_inside, top, bot);

        math::color rim = pal.border_inner;
        rim.a = 200;
        ctx.pnt().filled_rect({h_inside.x, h_inside.y, h_inside.w, ctx.pnt().hairline()}, rim);

        const float well_w = static_cast<float>(m.chevron_well_w);
        const rect chev_well{header.right() - well_w, header.y + 1.0f, well_w, header.h - 2.0f};

        const bool open = m_popup.is_open();
        if (open) {
            const float underline = static_cast<float>(ctx.theme().metrics().active_underline_h);
            ctx.pnt().filled_rect({h_inside.x, header.bottom() - underline, h_inside.w, underline}, ctx.theme().accent_value());
        }

        const int idx = m_bound->peek_base();
        const dh::string_view sel_text = (idx >= 0 && idx < static_cast<int>(m_options.size())) ?
            dh::string_view{m_options[static_cast<dh::size_t>(idx)].data(), m_options[static_cast<dh::size_t>(idx)].size()} :
            dh::string_view{"-"};
        const float text_inset = static_cast<float>(m.field_text_inset);
        const float text_x = header.x + text_inset;
        const float text_y = ctx.pnt().text_center_y(header);
        const math::color tc = (idx >= 0 && idx < static_cast<int>(m_options.size())) ? pal.text_primary : pal.text_muted;
        ctx.pnt().text_truncated({text_x, text_y}, sel_text, header.w - well_w - text_inset - static_cast<float>(m.text_truncate_margin), tc);

        const math::color chev = open ? ctx.theme().accent_value() : pal.text_secondary;
        ctx.pnt().chevron_down({chev_well.center().x, chev_well.center().y}, m.chevron_size, chev, pal.text_shadow);
    }

    bool combobox::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const bool inside_header = header_rect(ctx).contains(cursor);

        const bool blocked = ctx.cursor_in_popup();

        if (inside_header && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }
        m_hover_t.set_target((inside_header && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (m_context_editor != nullptr && m_context_editor->open_on_right_click(ctx, m_rect)) {
            m_popup.close();
            return true;
        }

        if (inside_header && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            if (m_popup.is_open()) {
                m_popup.close();
            } else {
                open_popup(ctx);
            }
            ctx.consume_mouse();
            return true;
        }

        if (inside_header && !m_popup.is_open() && ctx.input().key_down(VK_CONTROL) && !ctx.mouse_consumed() && !ctx.wheel_locked()) {
            const float w = ctx.input().mouse_wheel_delta();
            if (w != 0.0f && !m_options.empty()) {
                int next = m_bound->peek_base() - static_cast<int>(w);
                if (next < 0) next = 0;
                if (next >= static_cast<int>(m_options.size())) {
                    next = static_cast<int>(m_options.size()) - 1;
                }
                m_bound->set(next);

                ctx.consume_mouse();
                return true;
            }
        }
        return false;
    }

}
