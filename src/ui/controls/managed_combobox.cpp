// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "log/log.h"
#include "managed_combobox.h"

#include "log/log.h"
#include "ui/controls/search_match.h"
#include "ui/controls/text_input.h"
#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {
        constexpr int k_sep_h = 9;

        constexpr float k_sep_inset = 8.0f;

        enum class sign { plus, minus };

        void draw_sign(painter& pnt, rect btn, sign which, math::color c) noexcept {
            constexpr float arm = 8.0f;
            constexpr float thick = 2.0f;
            const point ctr = btn.center();
            pnt.filled_rect({ctr.x - arm * 0.5f, ctr.y - thick * 0.5f, arm, thick}, c);
            if (which == sign::plus) {
                pnt.filled_rect({ctr.x - thick * 0.5f, ctr.y - arm * 0.5f, thick, arm}, c);
            }
        }

        [[nodiscard]] dh::string_view as_view(const dh::string& s) noexcept {
            return dh::string_view{s.data(), s.size()};
        }
    }

    managed_combobox::managed_combobox(dh::string label, reactive::signal<int>& selected, reactive::signal<dh::u64>& active_mask, dh::vector<dh::string> options, int pinned_count, const i_option_catalog* catalog) noexcept : m_label(dh::move(label)), m_selected(&selected), m_active_mask(&active_mask), m_options(dh::move(options)), m_pinned_count(pinned_count) {
        if (m_pinned_count < 0) m_pinned_count = 0;
        if (m_pinned_count > static_cast<int>(m_options.size())) {
            m_pinned_count = static_cast<int>(m_options.size());
        }

        const dh::size_t max_options = static_cast<dh::size_t>(m_pinned_count) + k_max_option_set_options;
        if (m_options.size() > max_options) {
            ::dh::log::warn("too many options for {} ({} total, {} pinned, {} extra max)", dh::string_view{m_label.data(), m_label.size()}, m_options.size(), m_pinned_count, k_max_option_set_options);
            m_options.clear();
            m_pinned_count = 0;
        }

        if (catalog != nullptr) {
            const dh::size_t pinned = static_cast<dh::size_t>(m_pinned_count);
            catalog->publish_option_names(*m_active_mask, dh::span<const dh::string>{m_options.data() + pinned, m_options.size() - pinned});
        }

        if (searchable()) {
            m_search_input = adopt_child(dh::make_unique<text_input>(dh::string{}, m_filter, 64));
            m_search_input->set_placeholder("search...");
        }

        m_hover_t.snap(0.0f);

        m_selection_effect.emplace([this]() noexcept { snap_dropped_selection(); });
    }

    void managed_combobox::snap_dropped_selection() noexcept {
        const dh::u64 mask = m_active_mask->base();
        const int option = m_selected->base();
        if (!is_overridable(option) || option >= static_cast<int>(m_options.size())) return;
        if ((mask & option_bit(option)) != 0) return;

        select(m_pinned_count > 0 ? 0 : k_no_selection);
    }

    managed_combobox::~managed_combobox() = default;

    managed_combobox* managed_combobox::on_select(dh::function<void(int)> cb) noexcept {
        m_on_select = dh::move(cb);
        return this;
    }

    bool managed_combobox::is_overridable(int option) const noexcept {
        return option >= m_pinned_count;
    }

    dh::u64 managed_combobox::option_bit(int option) const noexcept {
        if (!is_overridable(option)) return 0;
        return dh::u64{1} << static_cast<unsigned>(option - m_pinned_count);
    }

    const dh::vector<managed_combobox::row>& managed_combobox::rows() noexcept {
        const dh::u64 mask = m_active_mask->peek_base();
        const dh::string& filter = m_filter.peek_base();
        if (m_rows_valid && mask == m_rows_mask && filter == m_rows_filter) return m_rows;

        if (filter != m_rows_filter) {
            m_scroll = 0.0f;
            m_popup_hover_row = -1;
        }
        m_rows_filter = filter;
        const dh::string_view needle{filter.data(), filter.size()};

        m_rows.clear();
        m_rows.reserve(m_options.size() + 1);

        for (int i = 0; i < m_pinned_count && i < static_cast<int>(m_options.size()); ++i) {
            if (option_contains_ci(as_view(m_options[static_cast<dh::size_t>(i)]), needle))
                m_rows.push_back({i, row_kind::pinned});
        }

        bool any_inactive = false;
        for (int i = m_pinned_count; i < static_cast<int>(m_options.size()); ++i) {
            if (!option_contains_ci(as_view(m_options[static_cast<dh::size_t>(i)]), needle)) continue;
            if ((mask & option_bit(i)) != 0)
                m_rows.push_back({i, row_kind::active});
            else
                any_inactive = true;
        }
        if (any_inactive) {
            if (!m_rows.empty()) m_rows.push_back({-1, row_kind::separator});
            for (int i = m_pinned_count; i < static_cast<int>(m_options.size()); ++i) {
                if ((mask & option_bit(i)) == 0 && option_contains_ci(as_view(m_options[static_cast<dh::size_t>(i)]), needle))
                    m_rows.push_back({i, row_kind::inactive});
            }
        }

        m_rows_mask = mask;
        m_rows_valid = true;
        return m_rows;
    }

    rect managed_combobox::field_rect(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        const float line_h = static_cast<float>(ctx.pnt().text_line_h());
        const float label_off = m_label.empty() ? 0.0f : line_h + static_cast<float>(m.label_row_gap);
        return {m_rect.x, m_rect.y + label_off, m_rect.w, static_cast<float>(m.control_h)};
    }

    size managed_combobox::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        const int line_h = ctx.pnt().text_line_h();
        const int label_off = m_label.empty() ? 0 : line_h + m.label_row_gap;
        return {0, label_off + m.control_h};
    }

    bool managed_combobox::searchable() const noexcept {
        return m_options.size() >= k_popup_search_min_options;
    }

    rect managed_combobox::list_bounds(const frame_ctx& ctx, rect popup) const noexcept {
        if (!searchable()) return popup;
        const float offset = static_cast<float>(ctx.theme().metrics().row_h + ctx.theme().metrics().gap_normal);
        return {popup.x, popup.y + offset, popup.w, popup.h - offset};
    }

    rect managed_combobox::search_bounds(const frame_ctx& ctx, rect popup) const noexcept {
        return {popup.x + list_popup_pad(ctx), popup.y + list_popup_pad(ctx),
            popup.w - list_popup_pad(ctx) * 2.0f, static_cast<float>(ctx.theme().metrics().row_h)};
    }

    float managed_combobox::row_height(const frame_ctx& ctx, row_kind kind) noexcept {
        return kind == row_kind::separator ? static_cast<float>(k_sep_h) : static_cast<float>(ctx.theme().metrics().popup_row_h);
    }

    float managed_combobox::content_height(const frame_ctx& ctx) noexcept {
        float total = 0.0f;
        for (const auto& rw : rows()) {
            total += row_height(ctx, rw.kind);
        }
        return total;
    }

    rect managed_combobox::compute_popup_bounds(const frame_ctx& ctx) noexcept {
        const rect field = field_rect(ctx);
        const float item_h = row_height(ctx, row_kind::active);

        const float max_content = static_cast<float>(k_max_visible_items) * item_h;
        const float content = content_height(ctx);
        const float viewport = dh::min(content > 0.0f ? content : item_h, max_content);
        const float search_h = searchable() ? static_cast<float>(ctx.theme().metrics().row_h + ctx.theme().metrics().gap_normal) : 0.0f;
        const float popup_h = list_popup_pad(ctx) * 2.0f + search_h + viewport;
        const float vp_h = static_cast<float>(ctx.pnt().viewport_h());

        float py = field.bottom();
        if (py + popup_h > vp_h) {
            py = field.y - popup_h;
            if (py < 0.0f) py = 0.0f;
        }
        return {field.x, py, field.w, popup_h};
    }

    scroll_metrics managed_combobox::bar_metrics(const frame_ctx& ctx, rect popup) noexcept {
        const auto& m = ctx.theme().metrics();
        const rect track = popup_scroll_track(popup, list_popup_pad(ctx), static_cast<float>(m.scroll_bar_w));
        return {.window = track.h,
            .total = content_height(ctx),
            .position = m_scroll,
            .track_start = track.y,
            .track_length = track.h,
            .thumb_min = static_cast<float>(m.scroll_thumb_min)};
    }

    void managed_combobox::clamp_scroll(const frame_ctx& ctx) noexcept {
        m_scroll = bar_metrics(ctx, list_bounds(ctx, m_popup.bounds())).clamped(m_scroll);
    }

    void managed_combobox::open_popup(frame_ctx& ctx) noexcept {
        m_filter.set(dh::string{});
        (void)rows();
        m_scroll = 0.0f;

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

    void managed_combobox::paint_popup(painter& pnt, frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();
        const rect r = m_popup.bounds();
        const rect list_bounds_value = list_bounds(ctx, r);

        pnt.chiseled_panel(r, pal.popup_border_outer, pal.popup_border_inner, pal.popup_bg);
        if (m_search_input) {
            m_search_input->arrange(search_bounds(ctx, r));
            m_search_input->paint(ctx);
        }

        const scroll_metrics bar = bar_metrics(ctx, list_bounds_value);
        const float bar_w = static_cast<float>(m.scroll_bar_w);
        const float content_x = r.x + 1.0f;

        const float content_w = r.w - 2.0f - (bar.overflows() ? bar_w + 2.0f : 0.0f);
        const float text_inset = static_cast<float>(m.popup_row_text_inset);

        const float button_w = static_cast<float>(m.chevron_well_w);

        const int selected = m_selected->peek_base();

        const rect viewport{list_bounds_value.x + 1.0f, list_bounds_value.y + list_popup_pad(ctx),
            list_bounds_value.w - 2.0f, list_bounds_value.h - list_popup_pad(ctx) * 2.0f};
        {
            const painter::clip_scope clip{pnt, viewport};

            const auto& list = rows();
            if (list.empty()) {
                pnt.text({content_x + text_inset, pnt.text_center_y({content_x, viewport.y, content_w, row_height(ctx, row_kind::active)})}, "no matches", pal.text_muted);
            }
            float y = viewport.y - m_scroll;
            for (dh::size_t i = 0; i < list.size(); ++i) {
                const auto& rw = list[i];
                if (rw.kind == row_kind::separator) {
                    math::color line = pal.popup_border_inner;
                    line.a = 120;
                    pnt.filled_rect({content_x + k_sep_inset, y + static_cast<float>(k_sep_h) * 0.5f, content_w - k_sep_inset * 2.0f, 1.0f}, line);
                    y += row_height(ctx, rw.kind);
                    continue;
                }

                const rect item_r{content_x, y, content_w, row_height(ctx, rw.kind)};
                const bool sel = rw.option == selected;
                const bool hov = m_popup_hover_row == static_cast<int>(i);
                const bool on_btn = hov && m_hover_on_button;
                const bool inactive = rw.kind == row_kind::inactive;

                if (sel) {
                    pnt.filled_rect(item_r, ctx.theme().accent_wash());
                    pnt.filled_rect({item_r.x, item_r.y, static_cast<float>(ctx.theme().metrics().row_stripe_w), item_r.h}, ctx.theme().accent_value());
                } else if (inactive ? on_btn : hov) {
                    pnt.filled_rect(item_r, pal.popup_row_hover);
                }

                const float btn_w = is_overridable(rw.option) ? button_w : 0.0f;
                const float tx = item_r.x + text_inset;
                const float ty = pnt.text_center_y(item_r);
                const math::color tc = inactive ? (on_btn ? pal.text_secondary : pal.text_muted) : ((sel || hov) ? pal.text_primary : pal.text_secondary);
                pnt.text_truncated({tx, ty}, as_view(m_options[static_cast<dh::size_t>(rw.option)]), item_r.w - text_inset - static_cast<float>(m.text_truncate_margin) - btn_w, tc);

                if (is_overridable(rw.option)) {
                    const rect btn{item_r.right() - button_w, item_r.y, button_w, item_r.h};
                    draw_sign(pnt, btn, inactive ? sign::plus : sign::minus, on_btn ? pal.text_primary : pal.text_muted);
                }
                y += item_r.h;
            }
        }

        if (bar.overflows()) {
            const rect track = popup_scroll_track(list_bounds_value, list_popup_pad(ctx), bar_w);
            pnt.filled_rect(track, pal.form_dark);
            pnt.filled_rect({track.x + 1.0f, bar.thumb_start(), track.w - 2.0f, bar.thumb_length()}, m_dragging_thumb ? pal.text_secondary : pal.text_muted);
        }
    }

    bool managed_combobox::input_popup(frame_ctx& ctx) noexcept {
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
            (void)rows();
        }
        m_popup.set_bounds(compute_popup_bounds(ctx));
        clamp_scroll(ctx);

        const rect bounds = m_popup.bounds();
        const rect list_bounds_value = list_bounds(ctx, bounds);
        const auto& m = ctx.theme().metrics();
        const scroll_metrics bar = bar_metrics(ctx, list_bounds_value);
        const float bar_w = static_cast<float>(m.scroll_bar_w);
        const float content_x = bounds.x + 1.0f;
        const float content_w = bounds.w - 2.0f - (bar.overflows() ? bar_w + 2.0f : 0.0f);
        const float button_w = static_cast<float>(m.chevron_well_w);
        const rect viewport{list_bounds_value.x + 1.0f, list_bounds_value.y + list_popup_pad(ctx),
            list_bounds_value.w - 2.0f, list_bounds_value.h - list_popup_pad(ctx) * 2.0f};

        if (bar.overflows()) {
            const rect track = popup_scroll_track(list_bounds_value, list_popup_pad(ctx), bar_w);

            if (viewport.contains(cursor) && !ctx.wheel_locked()) {
                const float wheel = ctx.input().mouse_wheel_delta();
                if (wheel != 0.0f) {
                    m_scroll -= wheel * row_height(ctx, row_kind::active);
                    clamp_scroll(ctx);
                    ctx.lock_wheel();
                }
            }

            if (ctx.input().key_pressed(VK_LBUTTON) && track.contains(cursor)) {
                m_dragging_thumb = true;
            }
            if (m_dragging_thumb) {
                if (!ctx.input().key_down(VK_LBUTTON)) {
                    m_dragging_thumb = false;
                } else {
                    m_scroll = bar.position_for_thumb_start(cursor.y - bar.thumb_length() * 0.5f);
                    ctx.consume_mouse();
                    return true;
                }
            }
        }

        const auto& list = rows();
        m_popup_hover_row = -1;
        m_hover_on_button = false;

        float y = viewport.y - m_scroll;
        for (dh::size_t i = 0; i < list.size(); ++i) {
            const float h = row_height(ctx, list[i].kind);
            const rect row_r{content_x, y, content_w, h};
            if (list[i].kind != row_kind::separator && row_r.contains(cursor) && viewport.contains(cursor)) {
                m_popup_hover_row = static_cast<int>(i);
                const rect btn{row_r.right() - button_w, row_r.y, button_w, row_r.h};
                m_hover_on_button = is_overridable(list[i].option) && btn.contains(cursor);
            }
            y += h;
        }

        if (ctx.input().key_pressed(VK_LBUTTON) && m_popup_hover_row >= 0) {
            const row rw = list[static_cast<dh::size_t>(m_popup_hover_row)];
            if (m_hover_on_button) {
                m_active_mask->set(m_active_mask->peek_base() ^ option_bit(rw.option));
                ctx.consume_mouse();
                return true;
            }

            // selecting a row doesnt add it; use the plus first.
            if (rw.kind == row_kind::inactive) {
                ctx.consume_mouse();
                return true;
            }
            select(rw.option);
            ctx.consume_mouse();
            if (m_search_input && ctx.focus().is_focused(m_search_input->id())) ctx.focus().clear_focus();
            m_popup.close();
            return true;
        }
        return bounds.contains(cursor);
    }

    void managed_combobox::select(int option) noexcept {
        const int chosen = (option >= 0 && option < static_cast<int>(m_options.size())) ? option : k_no_selection;
        m_selected->set(chosen);
        if (m_on_select) {
            m_on_select(chosen);
        }
    }

    void managed_combobox::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();

        if (!m_label.empty()) {
            ctx.pnt().text_truncated({m_rect.x, m_rect.y}, as_view(m_label), m_rect.w, pal.text_label);
        }

        const rect header = field_rect(ctx);
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

        const int idx = m_selected->peek_base();
        const dh::string_view sel_text =
            (idx >= 0 && idx < static_cast<int>(m_options.size())) ? as_view(m_options[static_cast<dh::size_t>(idx)]) : dh::string_view{"-"};
        const float text_inset = static_cast<float>(m.field_text_inset);
        const float text_x = header.x + text_inset;
        const float text_y = ctx.pnt().text_center_y(header);
        const math::color tc = (idx >= 0 && idx < static_cast<int>(m_options.size())) ? pal.text_primary : pal.text_muted;
        ctx.pnt().text_truncated({text_x, text_y}, sel_text, header.w - well_w - text_inset - static_cast<float>(m.text_truncate_margin), tc);

        const math::color chev = open ? ctx.theme().accent_value() : pal.text_secondary;
        ctx.pnt().chevron_down({chev_well.center().x, chev_well.center().y}, m.chevron_size, chev, pal.text_shadow);
    }

    bool managed_combobox::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const rect header = field_rect(ctx);
        const bool inside_header = header.contains(cursor);
        const bool blocked = ctx.cursor_in_popup();

        if (inside_header && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }
        m_hover_t.set_target((inside_header && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (inside_header && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            if (m_popup.is_open())
                m_popup.close();
            else
                open_popup(ctx);
            ctx.consume_mouse();
            return true;
        }
        return false;
    }

}
