// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "slider.h"

#include "core/dh/from_chars.h"
#include "core/dh/format.h"
#include "core/dh/string.h"
#include "core/dh/numeric_limits.h"
#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        constexpr float k_label_track_gap = 6.0f;

        constexpr float k_track_only_pad = 8.0f;

        constexpr float k_track_bottom_pad = 4.0f;

        constexpr float k_label_value_gap = 4.0f;

        constexpr float k_value_cell_pad = 10.0f;
        constexpr float k_value_cell_min_w = 24.0f;
        constexpr int k_max_buf_len = 12;

        struct format_parts final {
            dh::string prefix;
            dh::string numeric;
            dh::string suffix;
        };

        [[nodiscard]] dh::string substr_copy(const dh::string& s, dh::size_t pos, dh::size_t count) noexcept {
            if (pos >= s.size() || count == 0) {
                return dh::string{};
            }
            const dh::size_t remaining = s.size() - pos;
            const dh::size_t take = count < remaining ? count : remaining;
            return dh::string{s.data() + pos, take};
        }

        [[nodiscard]] format_parts split_format(const dh::string& fmt) noexcept {
            const dh::string_view sv{fmt.data(), fmt.size()};
            const auto field = dh::validate_number_format(sv, dh::numeric_limits<dh::size_t>::max());
            if (!field) return {dh::string{}, dh::string{}, fmt};
            const auto open = field->offset;
            const auto close = open + field->length;
            return {
                dh::format(sv.substr(0, open)),
                substr_copy(fmt, open, field->length),
                dh::format(sv.substr(close)),
            };
        }

        [[nodiscard]] dh::string_view as_view(const dh::string& s) noexcept {
            return dh::string_view{s.data(), s.size()};
        }
    }

    slider::slider(dh::string label, reactive::signal<float>& bound, float min, float max, float step, const i_context_editor_factory* editors) noexcept : m_label(dh::move(label)), m_bound(&bound), m_min(min), m_max(max), m_step(step) {
        if (editors != nullptr) m_context_editor = editors->for_number(*this, m_label, bound, min, max, step);
        m_hover_t.snap(0.0f);
    }

    slider::~slider() = default;

    void slider::set_format(dh::string fmt) noexcept {
        m_format = dh::move(fmt);
        if (m_context_editor) m_context_editor->set_number_format(m_format);
    }

    void slider::bind_format(const reactive::signal<dh::string>& source) noexcept {
        m_format_source = &source;
        sync_format();
    }

    void slider::sync_format() noexcept {
        if (m_format_source == nullptr) return;
        const dh::string& source = m_format_source->peek();
        const dh::string_view wanted = source.empty() ? k_default_format : dh::string_view{source.data(), source.size()};
        if (wanted == dh::string_view{m_format.data(), m_format.size()}) return;
        set_format(dh::string{wanted.data(), wanted.size()});
    }

    void slider::set_value_fn(dh::function<dh::string(float)> fn) noexcept {
        m_value_fn = dh::move(fn);
        if (m_context_editor) m_context_editor->set_number_value_fn(m_value_fn);
    }

    void slider::set_track_only(bool track_only) noexcept {
        m_track_only = track_only;
    }

    size slider::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();

        if (m_track_only) {
            const int track_height = m.slider_track_h + static_cast<int>(k_track_only_pad);
            return {0, track_height};
        }
        const int line_h = ctx.pnt().text_line_h();
        return {0, line_h + static_cast<int>(k_label_track_gap) + m.slider_track_h + static_cast<int>(k_track_bottom_pad)};
    }

    float slider::clamp_to_step(float v) const noexcept {
        if (v <= m_min) return m_min;
        if (v >= m_max) return m_max;
        if (m_step <= 0.0f) return v;
        const float n = dh::round((v - m_min) / m_step);
        const float q = m_min + n * m_step;
        if (q < m_min) return m_min;
        if (q > m_max) return m_max;
        return q;
    }

    void slider::set_from_pixel(float px, frame_ctx& ctx) noexcept {
        const rect track = track_rect(ctx);
        if (track.w <= 0.0f) return;
        const float ratio = dh::clamp((px - track.x) / track.w, 0.0f, 1.0f);
        const float v = m_min + (m_max - m_min) * ratio;
        m_bound->set(clamp_to_step(v));
    }

    float slider::value_cell_w(const frame_ctx& ctx) const noexcept {
        if (m_value_fn) {
            float widest = 0.0f;
            constexpr int k_samples = 8;
            for (int i = 0; i <= k_samples; ++i) {
                const float v = m_min + (m_max - m_min) * (static_cast<float>(i) / k_samples);
                const dh::string sample = m_value_fn(v);
                widest = dh::max(widest, ctx.pnt().measure_text_w(as_view(sample)));
            }
            return dh::max(widest + k_value_cell_pad, k_value_cell_min_w);
        }

        const format_parts fp = split_format(m_format);
        const dh::string fallback{"{}"};
        const dh::string& num_fmt = fp.numeric.empty() ? fallback : fp.numeric;
        auto measure = [&](float v) noexcept -> float {
            const dh::string s = dh::format(dh::string_view{num_fmt.data(), num_fmt.size()}, v);
            return ctx.pnt().measure_text_w(as_view(s));
        };
        const float widest = dh::max(measure(m_min), measure(m_max));
        return dh::max(widest + k_value_cell_pad, k_value_cell_min_w);
    }

    rect slider::track_rect(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        const float track_h = static_cast<float>(m.slider_track_h);

        if (m_track_only) {
            return {m_rect.x, m_rect.y + (m_rect.h - track_h) * 0.5f, m_rect.w, track_h};
        }

        const float line_h = static_cast<float>(ctx.pnt().text_line_h());
        return {m_rect.x, m_rect.y + line_h + k_label_track_gap, m_rect.w, track_h};
    }

    slider::value_cells slider::value_geometry(const frame_ctx& ctx, rect value_row) const noexcept {
        const float line_h = static_cast<float>(ctx.pnt().text_line_h());

        const format_parts fp = m_value_fn ? format_parts{} : split_format(m_format);
        const float prefix_w = fp.prefix.empty() ? 0.0f : ctx.pnt().measure_text_w(as_view(fp.prefix));
        const float suffix_w = fp.suffix.empty() ? 0.0f : ctx.pnt().measure_text_w(as_view(fp.suffix));

        const float cell_w = value_cell_w(ctx);
        const rect cell{value_row.right() - suffix_w - cell_w, value_row.y, cell_w, line_h};
        const rect click{cell.x - prefix_w, value_row.y, prefix_w + cell_w + suffix_w, line_h};
        return {cell, click, prefix_w, suffix_w};
    }

    float slider::inline_value_width(const frame_ctx& ctx) const noexcept {
        const format_parts parts = m_value_fn ? format_parts{} : split_format(m_format);
        const float prefix_width = parts.prefix.empty() ? 0.0f : ctx.pnt().measure_text_w(as_view(parts.prefix));
        const float suffix_width = parts.suffix.empty() ? 0.0f : ctx.pnt().measure_text_w(as_view(parts.suffix));
        return prefix_width + value_cell_w(ctx) + suffix_width;
    }

    rect slider::inline_value_row(const frame_ctx& ctx, rect bounds) const noexcept {
        const float width = inline_value_width(ctx);
        const float line_height = static_cast<float>(ctx.pnt().text_line_h());
        return {bounds.right() - width, ctx.pnt().text_center_y(bounds), width, line_height};
    }

    void slider::begin_edit(frame_ctx& ctx) noexcept {
        if (m_editing) return;
        m_editing = true;
        m_edit_buf.clear();
        m_blink_t = 0.0f;

        ctx.focus().set_text_focus(*this);
    }

    void slider::paint_inline_value(frame_ctx& ctx, rect bounds) noexcept {
        const float value = dh::clamp(m_bound->peek_base(), m_min, m_max);
        paint_value_row(ctx, value, inline_value_row(ctx, bounds));
    }

    bool slider::on_inline_value_input(frame_ctx& ctx, rect bounds) noexcept {
        if (!m_track_only || ctx.mouse_consumed() || ctx.cursor_in_popup()) return false;
        const rect value_row = inline_value_row(ctx, bounds);
        if (!value_geometry(ctx, value_row).click.contains(ctx.cursor())) return false;

        ctx.focus().set_hovered(*this);
        if (!ctx.input().key_pressed(VK_LBUTTON)) return false;

        begin_edit(ctx);
        ctx.consume_mouse();
        return true;
    }

    void slider::finish_inline_edit(frame_ctx& ctx) noexcept {
        if (!m_editing) return;
        commit_edit(ctx);
    }

    void slider::commit_edit(frame_ctx& ctx) noexcept {
        float parsed = m_bound->peek_base();

        if (m_edit_buf.empty() || m_edit_buf == "-" || m_edit_buf == ".") {
        } else {
            const char* first = m_edit_buf.data();
            const char* last = first + m_edit_buf.size();
            float tmp = 0.0f;

            const auto r = dh::from_chars(first, last, tmp);
            if (r && *r == static_cast<dh::ptrdiff_t>(last - first)) {
                parsed = tmp;
            }
        }
        m_bound->set(clamp_to_step(parsed));
        m_edit_buf.clear();
        m_editing = false;
        if (ctx.focus().is_focused(id())) ctx.focus().clear_focus();
    }

    void slider::apply_typed_chars(frame_ctx& ctx) noexcept {
        const bool ctrl = ctx.input().key_down(VK_CONTROL);

        if (ctx.input().key_pressed(VK_BACK)) {
            if (ctrl) {
                m_edit_buf.clear();
            } else if (!m_edit_buf.empty()) {
                m_edit_buf.pop_back();
            }
            m_blink_t = 0.0f;
            ctx.consume_keyboard();
        }

        auto already_has = [this](char c) noexcept {
            for (char x : m_edit_buf)
                if (x == c) return true;
            return false;
        };
        auto try_push = [&](char c) noexcept {
            if (static_cast<int>(m_edit_buf.size()) >= k_max_buf_len) return;
            m_edit_buf.push_back(c);
            m_blink_t = 0.0f;
        };

        for (unsigned int vk = 0x30; vk <= 0x39; ++vk) {
            if (ctx.input().key_pressed(vk)) {
                try_push(static_cast<char>(vk));
                ctx.consume_keyboard();
            }
        }

        for (unsigned int vk = VK_NUMPAD0; vk <= VK_NUMPAD9; ++vk) {
            if (ctx.input().key_pressed(vk)) {
                try_push(static_cast<char>('0' + (vk - VK_NUMPAD0)));
                ctx.consume_keyboard();
            }
        }

        if (!already_has('.')) {
            if (ctx.input().key_pressed(VK_OEM_PERIOD) || ctx.input().key_pressed(VK_DECIMAL)) {
                try_push('.');
                ctx.consume_keyboard();
            }
        }

        if (m_min < 0.0f && m_edit_buf.empty()) {
            if (ctx.input().key_pressed(VK_OEM_MINUS) || ctx.input().key_pressed(VK_SUBTRACT)) {
                m_edit_buf.push_back('-');
                m_blink_t = 0.0f;
                ctx.consume_keyboard();
            }
        }
    }

    void slider::paint(frame_ctx& ctx) noexcept {
        sync_format();
        const auto& pal = ctx.theme().colors();
        const float value = dh::clamp(m_bound->peek_base(), m_min, m_max);
        if (!m_track_only) paint_value_row(ctx, value, m_rect);
        const rect track = track_rect(ctx);
        ctx.pnt().filled_rect(track, pal.border_outer);
        const rect inside = track.inset(ctx.pnt().hairline());
        ctx.pnt().vertical_gradient(inside, pal.check_top, pal.check_bot);
        const float ratio = m_max > m_min ? (value - m_min) / (m_max - m_min) : 0.0f;
        const rect fill{inside.x, inside.y, inside.w * ratio, inside.h};
        ctx.pnt().vertical_gradient(fill, ctx.theme().accent_value(), math::lerp(pal.track_bot, ctx.theme().accent_value(), ctx.theme().metrics().accent_fill_mix));
    }

    void slider::paint_value_row(frame_ctx& ctx, float v_clamped, rect value_row) noexcept {
        const auto& pal = ctx.theme().colors();

        const dh::string vstr = dh::format(dh::string_view{m_format.data(), m_format.size()}, v_clamped);

        const float line_h = static_cast<float>(ctx.pnt().text_line_h());
        const float lab_y = value_row.y;

        const bool custom = static_cast<bool>(m_value_fn);
        const format_parts fp = custom ? format_parts{} : split_format(m_format);

        const value_cells vg = value_geometry(ctx, value_row);
        const rect val_cell = vg.cell;
        const float prefix_w = vg.prefix_w;
        const float lab_avail = dh::max(0.0f, vg.click.x - value_row.x - k_label_value_gap);

        const math::color lc = pal.text_label;
        const math::color vc = ctx.theme().accent_value();

        ctx.pnt().text_truncated({value_row.x, lab_y}, as_view(m_label), lab_avail, lc);

        if (!fp.prefix.empty()) {
            ctx.pnt().text({val_cell.x - prefix_w, lab_y}, as_view(fp.prefix), vc);
        }

        if (!fp.suffix.empty()) {
            ctx.pnt().text({val_cell.right(), lab_y}, as_view(fp.suffix), vc);
        }

        if (m_editing) {
            ctx.pnt().filled_rect(val_cell, pal.border_outer);
            ctx.pnt().filled_rect(val_cell.inset(ctx.pnt().hairline()), pal.track_bot);
        }

        dh::string shown;
        if (m_editing) {
            shown = m_edit_buf;
        } else if (custom) {
            shown = m_value_fn(v_clamped);
        } else if (!fp.numeric.empty()) {
            shown = dh::format(dh::string_view{fp.numeric.data(), fp.numeric.size()}, v_clamped);
        } else {
            shown = vstr;
        }
        const float shown_w = shown.empty() ? 0.0f : ctx.pnt().measure_text_w(as_view(shown));

        const rect text_clip = m_editing ? val_cell.inset(2.0f) : val_cell;
        const painter::clip_scope clip{ctx.pnt(), text_clip};
        const float text_x = (m_editing ? text_clip.right() - 1.0f : val_cell.right()) - shown_w;
        if (!shown.empty()) {
            ctx.pnt().text({text_x, lab_y}, as_view(shown), vc);
        }
        if (m_editing) {
            const float blink = dh::fmod(m_blink_t, 1.0f);
            if (blink < 0.5f) {
                const float caret_x = text_x + shown_w + 1.0f;
                ctx.pnt().filled_rect({caret_x, lab_y + 1.0f, 1.0f, line_h - 2.0f}, pal.text_primary);
            }
        }
    }

    bool slider::on_input(frame_ctx& ctx) noexcept {
        sync_format();
        const point cursor = ctx.cursor();
        const rect track = track_rect(ctx);

        const bool over_track = track.contains(cursor);
        const bool over_row = m_rect.contains(cursor);

        const bool over_value = !m_track_only && value_geometry(ctx, m_rect).click.contains(cursor);

        const bool blocked = ctx.cursor_in_popup();
        if ((over_track || over_value) && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }
        m_hover_t.set_target((over_track && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (m_context_editor && m_context_editor->open_on_right_click(ctx, m_rect)) {
            if (m_editing) {
                commit_edit(ctx);
            }
            return true;
        }

        if (ctx.focus().is_captured(id())) {
            if (ctx.input().key_down(VK_LBUTTON)) {
                set_from_pixel(cursor.x, ctx);
                ctx.consume_mouse();
                return true;
            }
            ctx.focus().release_capture();
            return true;
        }

        if (ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            if (over_track) {
                if (m_editing) commit_edit(ctx);
                ctx.focus().capture(*this);
                set_from_pixel(cursor.x, ctx);
                ctx.consume_mouse();
                return true;
            }
            if (over_value) {
                begin_edit(ctx);
                ctx.consume_mouse();
                return true;
            }

            if (m_editing) {
                commit_edit(ctx);
            }
        }

        if (m_editing) {
            m_blink_t += ctx.frame_dt();

            if (!ctx.focus().is_focused(id())) {
                commit_edit(ctx);
                return false;
            }
            if (ctx.input().key_pressed(VK_ESCAPE)) {
                m_editing = false;
                m_edit_buf.clear();
                ctx.focus().clear_focus();
                ctx.consume_keyboard();
                return true;
            }
            if (ctx.input().key_pressed(VK_RETURN)) {
                commit_edit(ctx);
                ctx.consume_keyboard();
                return true;
            }
            if (!ctx.keyboard_consumed()) {
                apply_typed_chars(ctx);
            }
            return true;
        }

        if (over_row && ctx.input().key_down(VK_CONTROL) && !ctx.mouse_consumed() && !ctx.wheel_locked()) {
            const float w = ctx.input().mouse_wheel_delta();
            if (w != 0.0f && m_step > 0.0f) {
                m_bound->set(clamp_to_step(m_bound->peek_base() + w * m_step));
                ctx.consume_mouse();
                return true;
            }
        }

        if (over_row && m_step > 0.0f && !blocked && !ctx.keyboard_consumed()) {
            float delta = 0.0f;
            if (ctx.input().key_pressed(VK_LEFT)) delta -= m_step;
            if (ctx.input().key_pressed(VK_RIGHT)) delta += m_step;
            if (delta != 0.0f) {
                m_bound->set(clamp_to_step(m_bound->peek_base() + delta));
                ctx.consume_keyboard();
                return true;
            }
        }
        return false;
    }

}
