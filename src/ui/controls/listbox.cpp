// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "listbox.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include "core/dh/string.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {
        constexpr int k_frame = 1;
        constexpr int k_inner_pad = 3;

        constexpr float k_chevron_inset = 11.0f;
        constexpr float k_header_text_inset = 20.0f;

        constexpr float k_item_inset = 12.0f;
        constexpr float k_item_inset_in_section = 30.0f;
    }

    listbox::listbox(reactive::signal<int>& bound, dh::vector<dh::string> items, int visible_rows) noexcept : m_bound(&bound), m_visible_rows(visible_rows) {
        if (m_visible_rows < 1) m_visible_rows = 1;
        set_items(dh::move(items));
    }

    void listbox::set_items(dh::vector<dh::string> items) noexcept {
        dh::vector<row> rows;
        rows.reserve(items.size());
        for (auto& text : items) {
            row r;
            r.text = dh::move(text);
            rows.push_back(dh::move(r));
        }
        set_rows(dh::move(rows));
    }

    void listbox::set_rows(dh::vector<row> rows) noexcept {
        m_rows = dh::move(rows);

        m_row_colors.clear();
        rebuild_displayed();

        if (m_hover_idx >= static_cast<int>(m_rows.size())) m_hover_idx = -1;

        if (m_bound->peek_base() >= static_cast<int>(m_rows.size())) m_bound->set(-1);
    }

    void listbox::set_placeholder(dh::string text) noexcept {
        m_placeholder = dh::move(text);
    }

    bool listbox::folded(dh::string_view header) const noexcept {
        for (const auto& name : m_folded) {
            if (dh::string_view{name.data(), name.size()} == header) return true;
        }
        return false;
    }

    void listbox::toggle_fold(dh::string_view header) noexcept {
        for (dh::size_t i = 0; i < m_folded.size(); ++i) {
            const auto& name = m_folded[i];
            if (dh::string_view{name.data(), name.size()} != header) continue;
            m_folded.erase(m_folded.begin() + static_cast<dh::ptrdiff_t>(i));
            rebuild_displayed();
            return;
        }
        m_folded.push_back(dh::string{header.data(), header.size()});
        rebuild_displayed();
    }

    void listbox::rebuild_displayed() noexcept {
        m_displayed.clear();
        m_has_headers = false;
        bool hiding = false;
        for (dh::size_t i = 0; i < m_rows.size(); ++i) {
            const row& r = m_rows[i];
            if (r.header) {
                m_has_headers = true;
                hiding = folded(dh::string_view{r.text.data(), r.text.size()});
                m_displayed.push_back(static_cast<int>(i));
                continue;
            }
            if (!hiding) m_displayed.push_back(static_cast<int>(i));
        }
    }

    void listbox::set_row_colors(dh::vector<dh::optional<math::color>> colors) noexcept {
        m_row_colors = dh::move(colors);
    }

    int listbox::selected() const noexcept {
        const int idx = m_bound->peek_base();
        if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return -1;
        return m_rows[static_cast<dh::size_t>(idx)].header ? -1 : idx;
    }

    dh::string_view listbox::selected_name() const noexcept {
        const int idx = selected();
        if (idx < 0) return {};
        const dh::string& text = m_rows[static_cast<dh::size_t>(idx)].text;
        return dh::string_view{text.data(), text.size()};
    }

    void listbox::select(dh::string_view name) noexcept {
        for (dh::size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].header) continue;
            if (m_rows[i].text == name) {
                m_bound->set(static_cast<int>(i));
                return;
            }
        }
        m_bound->set(-1);
    }

    void listbox::scroll_to_selected() noexcept {
        const int idx = selected();
        if (idx < 0) return;

        for (dh::size_t i = 0; i < m_displayed.size(); ++i) {
            if (m_displayed[i] != idx) continue;

            m_first_visible = dh::max(0, static_cast<int>(i) - 1);
            return;
        }
    }

    size listbox::desired_size(const frame_ctx& ctx) const noexcept {
        const int row_h = ctx.theme().metrics().listbox_row_h;
        const int h = (k_frame + k_inner_pad) * 2 + row_h * m_visible_rows;
        return {0, h};
    }

    int listbox::visible_count(const frame_ctx& ctx) const noexcept {
        const int row_h = ctx.theme().metrics().listbox_row_h;
        const int avail = static_cast<int>(m_rect.h) - (k_frame + k_inner_pad) * 2;
        if (avail <= 0 || row_h <= 0) return 0;
        return dh::min(static_cast<int>(m_displayed.size()), avail / row_h);
    }

    void listbox::clamp_scroll(const frame_ctx& ctx) noexcept {
        const int vis = visible_count(ctx);
        const int max_first = dh::max(0, static_cast<int>(m_displayed.size()) - vis);
        if (m_first_visible < 0) m_first_visible = 0;
        if (m_first_visible > max_first) m_first_visible = max_first;
    }

    listbox::layout listbox::geometry(const frame_ctx& ctx) noexcept {
        clamp_scroll(ctx);

        const auto& metrics = ctx.theme().metrics();
        const float bar_w = static_cast<float>(metrics.scroll_bar_w);

        layout g;
        g.inside = m_rect.inset(k_frame + k_inner_pad);
        g.visible = visible_count(ctx);
        g.bar = {.window = static_cast<float>(g.visible),
            .total = static_cast<float>(m_displayed.size()),
            .position = static_cast<float>(m_first_visible),
            .track_start = g.inside.y,
            .track_length = g.inside.h,
            .thumb_min = static_cast<float>(metrics.scroll_thumb_min)};
        g.content_w = g.bar.overflows() ? g.inside.w - (bar_w + 2.0f) : g.inside.w;

        if (g.bar.overflows()) {
            g.track = {g.inside.right() - bar_w, g.inside.y, bar_w, g.inside.h};
        }
        return g;
    }

    void listbox::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const auto& m = ctx.theme().metrics();

        ctx.pnt().chiseled_panel(m_rect, pal.popup_border_outer, pal.popup_border_inner, pal.list_bg);

        const layout g = geometry(ctx);
        const rect inside = g.inside;
        if (inside.w <= 0.0f || inside.h <= 0.0f) return;

        const int vis = g.visible;
        const float content_w = g.content_w;

        const float truncate_margin = static_cast<float>(m.text_truncate_margin);

        {
            const painter::clip_scope clip{ctx.pnt(), inside};

            const int sel_idx = selected();
            const float row_h_f = static_cast<float>(m.listbox_row_h);
            for (int i = 0; i < vis; ++i) {
                const int slot = m_first_visible + i;
                if (slot >= static_cast<int>(m_displayed.size())) break;
                const int idx = m_displayed[static_cast<dh::size_t>(slot)];
                const row& r = m_rows[static_cast<dh::size_t>(idx)];
                const rect row_rect{inside.x, inside.y + row_h_f * static_cast<float>(i), content_w, row_h_f};
                const bool sel = (idx == sel_idx);
                const bool hov = (m_hover_idx == idx);

                if (hov && !sel) ctx.pnt().filled_rect(row_rect, pal.list_row_hover);
                if (sel) {
                    ctx.pnt().filled_rect(row_rect, ctx.theme().accent_wash());
                    ctx.pnt().filled_rect({row_rect.x, row_rect.y, static_cast<float>(ctx.theme().metrics().row_stripe_w), row_rect.h}, ctx.theme().accent_value());
                }

                const dh::string_view text{r.text.data(), r.text.size()};
                const float ty = ctx.pnt().text_center_y(row_rect);

                if (r.header) {
                    const bool shut = folded(text);
                    const math::color glyph = hov ? pal.text_primary : pal.text_label;
                    const point center{row_rect.x + k_chevron_inset, row_rect.y + row_rect.h * 0.5f};
                    if (shut) {
                        ctx.pnt().chevron_right(center, m.chevron_size, glyph, pal.text_shadow);
                    } else {
                        ctx.pnt().chevron_down(center, m.chevron_size, glyph, pal.text_shadow);
                    }
                    const float hx = row_rect.x + k_header_text_inset;
                    ctx.pnt().text_truncated({hx, ty}, text, row_rect.right() - hx - truncate_margin, glyph);
                    continue;
                }

                const dh::size_t ci = static_cast<dh::size_t>(idx);
                const bool tinted = ci < m_row_colors.size() && m_row_colors[ci].has_value();
                const math::color tc = tinted ? *m_row_colors[ci] : (sel || hov ? pal.text_primary : pal.text_secondary);

                const float tx = row_rect.x + (m_has_headers ? k_item_inset_in_section : k_item_inset);
                const dh::string_view annotation{r.annotation.data(), r.annotation.size()};
                float text_right = row_rect.right() - truncate_margin;
                if (!annotation.empty()) {
                    const float annotation_w = ctx.pnt().measure_text_w(annotation);
                    const float annotation_x = text_right - annotation_w;
                    ctx.pnt().text({annotation_x, ty}, annotation, sel || hov ? pal.text_secondary : pal.text_muted);
                    text_right = annotation_x - static_cast<float>(m.listbox_annotation_gap);
                }
                ctx.pnt().text_truncated({tx, ty}, text, dh::max(0.0f, text_right - tx), tc);
            }

            if (m_displayed.empty() && !m_placeholder.empty()) {
                const rect first{inside.x, inside.y, content_w, row_h_f};
                ctx.pnt().text_truncated({first.x + k_item_inset, ctx.pnt().text_center_y(first)}, dh::string_view{m_placeholder.data(), m_placeholder.size()}, first.w - k_item_inset - truncate_margin, pal.text_placeholder);
            }
        }

        if (g.bar.overflows()) {
            ctx.pnt().filled_rect(g.track, pal.form_dark);
            ctx.pnt().filled_rect({g.track.x + 1.0f, g.bar.thumb_start(), g.track.w - 2.0f, g.bar.thumb_length()}, m_dragging_thumb ? pal.text_secondary : pal.text_muted);
        }
    }

    bool listbox::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const auto& m = ctx.theme().metrics();

        const layout g = geometry(ctx);
        const rect inside = g.inside;
        const int vis = g.visible;

        const bool blocked = ctx.cursor_in_popup();
        const bool inside_rect = !blocked && inside.contains(cursor);

        m_hover_idx = -1;
        if (inside_rect && cursor.x < inside.x + g.content_w) {
            const int yi = static_cast<int>((cursor.y - inside.y) / static_cast<float>(m.listbox_row_h));
            const int slot = m_first_visible + yi;
            if (yi >= 0 && yi < vis && slot >= 0 && slot < static_cast<int>(m_displayed.size())) {
                m_hover_idx = m_displayed[static_cast<dh::size_t>(slot)];
            }
        }

        if (inside_rect && !ctx.mouse_consumed()) {
            ctx.focus().set_hovered(*this);
        }

        if (inside_rect && !ctx.mouse_consumed() && !ctx.wheel_locked()) {
            const float w = ctx.input().mouse_wheel_delta();
            if (w != 0.0f) {
                m_first_visible -= static_cast<int>(w);
                clamp_scroll(ctx);

                ctx.consume_mouse();
                return true;
            }
        }

        if (m_hover_idx >= 0 && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            const row& hit = m_rows[static_cast<dh::size_t>(m_hover_idx)];
            if (hit.header) {
                toggle_fold(dh::string_view{hit.text.data(), hit.text.size()});
            } else {
                m_bound->set(m_hover_idx);
            }
            ctx.consume_mouse();
            return true;
        }

        if (g.bar.overflows()) {
            if (ctx.focus().is_captured(id())) {
                if (!ctx.input().key_down(VK_LBUTTON)) {
                    ctx.focus().release_capture();
                    m_dragging_thumb = false;
                } else {
                    m_first_visible = g.bar.row_position_for_thumb_start(cursor.y - m_drag_thumb_offset);
                    clamp_scroll(ctx);
                    ctx.consume_mouse();
                    return true;
                }
            }

            if (g.track.contains(cursor) && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                if (g.bar.thumb_holds(cursor.y)) {
                    m_drag_thumb_offset = cursor.y - g.bar.thumb_start();
                } else {
                    m_drag_thumb_offset = g.bar.thumb_length() * 0.5f;
                }
                ctx.focus().capture(*this);
                m_dragging_thumb = true;
                ctx.consume_mouse();
                return true;
            }
        }
        return false;
    }

}
