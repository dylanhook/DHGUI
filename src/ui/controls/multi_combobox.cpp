// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "log/log.h"
#include "multi_combobox.h"

#include "log/log.h"
#include "ui/controls/color_picker.h"
#include "ui/controls/search_match.h"
#include "ui/controls/text_input.h"
#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include "core/dh/unique_ptr.h"

#include <Windows.h>
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    multi_combobox::multi_combobox(dh::string label, reactive::signal<dh::u64>& bound, dh::vector<dh::string> options, const i_context_editor_factory* editors) noexcept : m_label(dh::move(label)), m_bound(&bound), m_options(dh::move(options)) {
        if (m_options.size() > k_max_option_set_options) {
            ::dh::log::warn("too many options for {} ({} / {})", dh::string_view{m_label.data(), m_label.size()}, m_options.size(), k_max_option_set_options);
            m_options.clear();
        }
        if (editors != nullptr) m_context_editor = editors->for_option_set(*this, m_label, bound, m_options);
        for (dh::size_t i = 0; i < m_options.size(); ++i)
            m_filtered_indices.push_back(static_cast<int>(i));
        if (searchable()) {
            m_search_input = adopt_child(dh::make_unique<text_input>(dh::string{}, m_filter, 64));
            m_search_input->set_placeholder("search...");
        }
        m_hover_t.snap(0.0f);

        const dh::u64 initial = m_bound->peek_base();
        m_item_check_t.reserve(m_options.size());
        for (dh::size_t i = 0; i < m_options.size(); ++i) {
            auto t = dh::make_unique<animation::tween>();
            if (!t) dh::alloc_failed("ui::controls::multi_combobox item tween");
            t->snap(((initial >> i) & 1u) ? 1.0f : 0.0f);
            m_item_check_t.push_back(dh::move(t));
        }
    }

    multi_combobox::~multi_combobox() = default;

    multi_combobox* multi_combobox::with_color(reactive::signal<math::color>& bound) noexcept {
        return with_color(bound, dh::function<bool()>{});
    }

    multi_combobox* multi_combobox::with_color(reactive::signal<math::color>& bound, dh::function<bool()> shown) noexcept {
        if (m_color_count >= k_max_inline_colors) return this;

        m_colors[m_color_count] = adopt_child(dh::make_unique<color_picker>(dh::string{}, bound));
        if (!m_colors[m_color_count]) dh::alloc_failed("ui::controls::multi_combobox swatch");
        m_color_shown[m_color_count] = dh::move(shown);
        ++m_color_count;
        return this;
    }

    bool multi_combobox::color_shown(dh::size_t index) const noexcept {
        return index < m_color_count && (!m_color_shown[index] || m_color_shown[index]());
    }

    void multi_combobox::set_placeholder(dh::string p) noexcept {
        m_placeholder = dh::move(p);
    }

    bool multi_combobox::searchable() const noexcept {
        return m_options.size() >= k_popup_search_min_options;
    }

    void multi_combobox::refresh_filtered() noexcept {
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

    int multi_combobox::option_at(int filtered_row) const noexcept {
        return filtered_row >= 0 && filtered_row < static_cast<int>(m_filtered_indices.size()) ?
            m_filtered_indices[static_cast<dh::size_t>(filtered_row)] : -1;
    }

    int multi_combobox::visible_items() const noexcept {
        const int count = static_cast<int>(m_filtered_indices.size());
        return count < k_max_visible_items ? count : k_max_visible_items;
    }

    bool multi_combobox::overflows() const noexcept {
        return static_cast<int>(m_filtered_indices.size()) > k_max_visible_items;
    }

    void multi_combobox::clamp_scroll() noexcept {
        const int max_first = static_cast<int>(m_filtered_indices.size()) - visible_items();
        if (m_first_visible > max_first) m_first_visible = max_first;
        if (m_first_visible < 0) m_first_visible = 0;
    }

    popup_scrollbar multi_combobox::scrollbar(const frame_ctx& ctx, rect popup) const noexcept {
        const auto& m = ctx.theme().metrics();
        return {.popup = popup,
            .pad = list_popup_pad(ctx),
            .bar_w = static_cast<float>(m.scroll_bar_w),
            .thumb_min = static_cast<float>(m.scroll_thumb_min),
            .visible = visible_items(),
            .total = static_cast<int>(m_filtered_indices.size()),
            .first_visible = m_first_visible};
    }

    rect multi_combobox::list_bounds(const frame_ctx& ctx, rect popup) const noexcept {
        if (!searchable()) return popup;
        const float offset = static_cast<float>(ctx.theme().metrics().row_h + ctx.theme().metrics().gap_normal);
        return {popup.x, popup.y + offset, popup.w, popup.h - offset};
    }

    rect multi_combobox::search_bounds(const frame_ctx& ctx, rect popup) const noexcept {
        return {popup.x + list_popup_pad(ctx), popup.y + list_popup_pad(ctx), popup.w - list_popup_pad(ctx) * 2.0f,
            static_cast<float>(ctx.theme().metrics().row_h)};
    }

    float multi_combobox::arrange_colors(const frame_ctx& ctx) noexcept {
        const float line_h = static_cast<float>(ctx.pnt().text_line_h());
        const float gap = static_cast<float>(ctx.theme().metrics().gap_normal);
        float right = m_rect.right();
        bool any = false;
        for (dh::size_t i = 0; i < m_color_count; ++i) {
            if (!color_shown(i)) continue;
            const float w = static_cast<float>(m_colors[i]->desired_size(ctx).w);
            if (any) right -= gap;
            m_colors[i]->arrange({right - w, m_rect.y, w, line_h});
            right -= w;
            any = true;
        }
        return any ? m_rect.right() - right + gap : 0.0f;
    }

    size multi_combobox::desired_size(const frame_ctx& ctx) const noexcept {
        return {0, label_offset(ctx) + ctx.theme().metrics().control_h};
    }

    int multi_combobox::label_offset(const frame_ctx& ctx) const noexcept {
        if (m_label.empty() && m_color_count == 0) return 0;
        return ctx.pnt().text_line_h() + ctx.theme().metrics().label_row_gap;
    }

    const dh::string& multi_combobox::joined_selection() noexcept {
        const dh::u64 mask = m_bound->peek_base();
        if (m_joined_valid && mask == m_joined_mask) return m_joined;

        m_joined.clear();
        bool first = true;
        for (dh::size_t i = 0; i < m_options.size(); ++i) {
            if ((mask >> i) & 1u) {
                if (!first) m_joined.append(", ");
                m_joined.append(m_options[i]);
                first = false;
            }
        }
        m_joined_mask = mask;
        m_joined_valid = true;
        return m_joined;
    }

    rect multi_combobox::header_rect(const frame_ctx& ctx) const noexcept {
        return {m_rect.x, m_rect.y + static_cast<float>(label_offset(ctx)), m_rect.w, static_cast<float>(ctx.theme().metrics().control_h)};
    }

    rect multi_combobox::compute_popup_bounds(const frame_ctx& ctx) const noexcept {
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

    void multi_combobox::open_popup(frame_ctx& ctx) noexcept {
        m_filter.set(dh::string{});
        refresh_filtered();
        clamp_scroll();

        popup_record rec;
        rec.z_band = ctx.theme().metrics().popup_z_combobox;
        rec.bounds = compute_popup_bounds(ctx);
        rec.draw = [this](painter& p, frame_ctx& f) noexcept { paint_popup(p, f); };
        rec.input = [this](frame_ctx& f) noexcept { return input_popup(f); };
        rec.on_dismissed = [this, focus = &ctx.focus()]() noexcept {
            if (m_search_input && focus->is_focused(m_search_input->id())) focus->clear_focus();
        };
        m_popup.open(ctx, static_cast<popup_record&&>(rec));
        if (m_search_input) ctx.focus().set_text_focus(*m_search_input);
    }

    void multi_combobox::paint_popup(painter& pnt, frame_ctx& ctx) noexcept {
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

        const dh::u64 mask = m_bound->peek_base();

        const int shown = visible_items();
        if (shown == 0) {
            const rect empty_row{content_x, content_y, content_w, item_h};
            pnt.text({empty_row.x + text_inset, pnt.text_center_y(empty_row)}, "no matches", pal.text_muted);
        }
        for (int row = 0; row < shown; ++row) {
            const int option = option_at(m_first_visible + row);
            if (option < 0) break;
            const dh::size_t i = static_cast<dh::size_t>(option);
            const rect item_r{content_x, content_y + item_h * static_cast<float>(row), content_w, item_h};

            const bool sel = ((mask >> i) & 1u) != 0;
            const bool hov = (m_popup_hover_idx == static_cast<int>(i));

            m_item_check_t[i]->set_target(sel ? 1.0f : 0.0f, ctx.theme().times().hover);
            const float k = m_item_check_t[i]->value();

            if (hov && k < 1.0f) {
                math::color hov_bg = pal.popup_row_hover;
                hov_bg.a = static_cast<dh::u8>(static_cast<float>(hov_bg.a) * (1.0f - k));
                pnt.filled_rect(item_r, hov_bg);
            }
            if (k > 0.0f) {
                math::color wash = ctx.theme().accent_wash();
                wash.a = static_cast<dh::u8>(static_cast<float>(wash.a) * k);
                pnt.filled_rect(item_r, wash);

                math::color stripe = ctx.theme().accent_value();
                stripe.a = static_cast<dh::u8>(255.0f * k);
                pnt.filled_rect({item_r.x, item_r.y, static_cast<float>(ctx.theme().metrics().row_stripe_w), item_r.h}, stripe);
            }

            const float tx = item_r.x + text_inset;
            const float ty = pnt.text_center_y(item_r);

            const math::color rest = hov ? pal.text_primary : pal.text_secondary;
            const math::color tc = math::lerp(rest, pal.text_primary, k);
            const dh::string& opt = m_options[i];
            pnt.text_truncated({tx, ty}, dh::string_view{opt.data(), opt.size()}, item_r.w - text_inset - static_cast<float>(m.text_truncate_margin), tc);
        }

        if (overflows()) {
            const rect track = sb.track();
            pnt.filled_rect(track, pal.form_dark);
            pnt.filled_rect({track.x + 1.0f, sb.bar().thumb_start(), track.w - 2.0f, sb.bar().thumb_length()}, m_dragging_thumb ? pal.text_secondary : pal.text_muted);
        }
    }

    bool multi_combobox::input_popup(frame_ctx& ctx) noexcept {
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
            const dh::u64 bit = dh::u64{1} << m_popup_hover_idx;
            m_bound->set(m_bound->peek_base() ^ bit);
            ctx.consume_mouse();
            return true;
        }
        return inside;
    }

    void multi_combobox::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();

        const float label_w = m_rect.w - arrange_colors(ctx);
        ctx.pnt().text_truncated({m_rect.x, m_rect.y}, dh::string_view{m_label.data(), m_label.size()}, label_w, pal.text_label);

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

        const dh::string& joined = joined_selection();
        const dh::string& shown = joined.empty() ? m_placeholder : joined;
        const dh::string_view display{shown.data(), shown.size()};
        const float text_inset = static_cast<float>(m.field_text_inset);
        const float text_x = header.x + text_inset;
        const float text_y = ctx.pnt().text_center_y(header);
        const math::color tc = joined.empty() ? pal.text_muted : pal.text_primary;
        ctx.pnt().text_truncated({text_x, text_y}, display, header.w - well_w - text_inset - static_cast<float>(m.text_truncate_margin), tc);

        const math::color chev = open ? ctx.theme().accent_value() : pal.text_secondary;
        ctx.pnt().chevron_down({chev_well.center().x, chev_well.center().y}, m.chevron_size, chev, pal.text_shadow);

        for (dh::size_t i = 0; i < m_color_count; ++i) {
            if (color_shown(i)) m_colors[i]->paint(ctx);
        }
    }

    bool multi_combobox::on_input(frame_ctx& ctx) noexcept {
        (void)arrange_colors(ctx);
        for (dh::size_t i = 0; i < m_color_count; ++i) {
            if (color_shown(i) && m_colors[i]->on_input(ctx)) return true;
        }

        const point cursor = ctx.cursor();
        const bool inside_header = header_rect(ctx).contains(cursor);

        const bool blocked = ctx.cursor_in_popup();

        if (inside_header && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }
        m_hover_t.set_target((inside_header && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (m_context_editor && m_context_editor->open_on_right_click(ctx, m_rect)) {
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
        return false;
    }

}
