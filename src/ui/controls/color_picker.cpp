// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "color_picker.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include "core/dh/optional.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        constexpr int k_inner_pad = 4;
        constexpr int k_sv_size = 132;
        constexpr int k_hue_w = 12;
        constexpr int k_strip_gap = 4;
        constexpr int k_alpha_h = 8;

        constexpr int k_picker_w = k_sv_size + k_strip_gap + k_hue_w + k_inner_pad * 2;
        constexpr int k_picker_h = k_sv_size + k_strip_gap + k_alpha_h + k_inner_pad * 2;

        constexpr int k_picker_h_no_alpha = k_sv_size + k_inner_pad * 2;

        constexpr int k_ctx_w = 110;

        enum class context_action : int { copy, paste, reset, count };

        constexpr int k_ctx_items = static_cast<int>(context_action::count);

        constexpr dh::string_view k_ctx_labels[static_cast<dh::size_t>(k_ctx_items)] = {
            "copy",
            "paste",
            "reset",
        };

        constexpr int k_hue_strip_segments = k_sv_size;

        constexpr dh::u8 k_strip_cursor_alpha = 220;

        constinit dh::optional<math::color> g_clipboard{};

        [[nodiscard]] math::color hsv_to_rgb(float h, float s, float v, float a) noexcept {
            h = dh::fmod(h, 360.0f);
            if (h < 0.0f) h += 360.0f;
            const float c = v * s;
            const float hp = h / 60.0f;
            const float x = c * (1.0f - dh::fabs(dh::fmod(hp, 2.0f) - 1.0f));
            float r = 0, g = 0, b = 0;
            if (hp < 1.0f) {
                r = c;
                g = x;
            } else if (hp < 2.0f) {
                r = x;
                g = c;
            } else if (hp < 3.0f) {
                g = c;
                b = x;
            } else if (hp < 4.0f) {
                g = x;
                b = c;
            } else if (hp < 5.0f) {
                r = x;
                b = c;
            } else {
                r = c;
                b = x;
            }
            const float m = v - c;
            return {
                static_cast<dh::u8>(dh::clamp((r + m) * 255.0f, 0.0f, 255.0f)),
                static_cast<dh::u8>(dh::clamp((g + m) * 255.0f, 0.0f, 255.0f)),
                static_cast<dh::u8>(dh::clamp((b + m) * 255.0f, 0.0f, 255.0f)),
                static_cast<dh::u8>(dh::clamp(a * 255.0f, 0.0f, 255.0f)),
            };
        }

        void rgb_to_hsv(math::color c, float& h, float& s, float& v) noexcept {
            const float r = c.r_norm();
            const float g = c.g_norm();
            const float b = c.b_norm();
            const float mx = dh::max(r, dh::max(g, b));
            const float mn = dh::min(r, dh::min(g, b));
            const float d = mx - mn;
            v = mx;
            s = (mx <= 0.0f) ? 0.0f : d / mx;
            if (d == 0.0f) {
                h = 0.0f;
            } else if (mx == r) {
                h = 60.0f * dh::fmod(((g - b) / d), 6.0f);
            } else if (mx == g) {
                h = 60.0f * (((b - r) / d) + 2.0f);
            } else {
                h = 60.0f * (((r - g) / d) + 4.0f);
            }
            if (h < 0.0f) h += 360.0f;
        }

    }

    color_picker::color_picker(dh::string label, reactive::signal<math::color>& bound) noexcept : m_label(dh::move(label)), m_bound(&bound) {
        m_last_synced = bound.peek();
        sync_hsv_from_color();
        m_initial_color = bound.peek();
    }

    math::color color_picker::displayed_color() const noexcept {
        if (m_commit_on_release && m_drag != drag::none) {
            return hsv_to_rgb(m_hue, m_sat, m_val, m_alpha_enabled ? m_alpha : 1.0f);
        }
        return m_bound->peek();
    }

    void color_picker::sync_hsv_from_color() noexcept {
        const auto c = m_bound->peek();
        rgb_to_hsv(c, m_hue, m_sat, m_val);
        m_alpha = c.a_norm();
        m_last_synced = c;
    }

    void color_picker::publish_color() noexcept {
        const auto c = hsv_to_rgb(m_hue, m_sat, m_val, m_alpha_enabled ? m_alpha : 1.0f);
        m_bound->set(c);
        m_last_synced = c;
    }

    size color_picker::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        if (m_label.empty()) {
            return {m.swatch_w + 4, m.row_h};
        }
        return {0, m.row_h};
    }

    rect color_picker::compute_swatch_rect_ctx(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();
        const float sw = static_cast<float>(m.swatch_w);
        const float sh = static_cast<float>(m.swatch_h);

        const float x = m_label.empty() ? m_rect.x + (m_rect.w - sw) * 0.5f : m_rect.right() - sw - 2.0f;
        return {x, m_rect.y + (m_rect.h - sh) * 0.5f, sw, sh};
    }

    color_picker::strips color_picker::picker_strips(rect popup) const noexcept {
        strips out;
        out.saturation_value = {
            popup.x + static_cast<float>(k_inner_pad), popup.y + static_cast<float>(k_inner_pad), static_cast<float>(k_sv_size), static_cast<float>(k_sv_size)};
        out.hue = {out.saturation_value.right() + static_cast<float>(k_strip_gap), out.saturation_value.y, static_cast<float>(k_hue_w), out.saturation_value.h};
        out.alpha = {
            out.saturation_value.x, out.saturation_value.bottom() + static_cast<float>(k_strip_gap), out.saturation_value.w, static_cast<float>(k_alpha_h)};
        return out;
    }

    rect color_picker::compute_picker_bounds(const frame_ctx& ctx) const noexcept {
        const rect sw = compute_swatch_rect_ctx(ctx);
        rect r{sw.x + sw.w - static_cast<float>(k_picker_w), sw.bottom() + 4.0f, static_cast<float>(k_picker_w),
            static_cast<float>(m_alpha_enabled ? k_picker_h : k_picker_h_no_alpha)};
        const float vp_w = static_cast<float>(ctx.pnt().viewport_w());
        const float vp_h = static_cast<float>(ctx.pnt().viewport_h());
        if (r.right() > vp_w) r.x = vp_w - r.w;
        if (r.bottom() > vp_h) r.y = sw.y - r.h - 4.0f;
        if (r.x < 0) r.x = 0;
        if (r.y < 0) r.y = 0;
        return r;
    }

    void color_picker::open_picker(frame_ctx& ctx) noexcept {
        sync_hsv_from_color();

        popup_record rec;
        rec.z_band = ctx.theme().metrics().popup_z_picker;
        rec.bounds = compute_picker_bounds(ctx);
        rec.draw = [this](painter& p, frame_ctx& f) noexcept { paint_picker(p, f); };
        rec.input = [this](frame_ctx& f) noexcept { return input_picker(f); };

        rec.on_dismissed = [this]() noexcept {
            if (m_drag != drag::none && m_commit_on_release) publish_color();
            m_drag = drag::none;
        };
        m_picker.open(ctx, static_cast<popup_record&&>(rec));
    }

    void color_picker::close_picker() noexcept {
        m_drag = drag::none;
        m_picker.close();
    }

    void color_picker::open_context(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        rect r{cursor.x, cursor.y, static_cast<float>(k_ctx_w), list_popup_pad(ctx) * 2.0f + static_cast<float>(ctx.theme().metrics().popup_row_h * k_ctx_items)};
        const float vp_w = static_cast<float>(ctx.pnt().viewport_w());
        const float vp_h = static_cast<float>(ctx.pnt().viewport_h());
        if (r.right() > vp_w) r.x = vp_w - r.w;
        if (r.bottom() > vp_h) r.y = vp_h - r.h;
        if (r.x < 0) r.x = 0;
        if (r.y < 0) r.y = 0;

        close_picker();

        popup_record rec;
        rec.z_band = ctx.theme().metrics().popup_z_picker_menu;
        rec.bounds = r;
        rec.draw = [this](painter& p, frame_ctx& f) noexcept { paint_context(p, f); };
        rec.input = [this](frame_ctx& f) noexcept { return input_context(f); };
        m_context.open(ctx, static_cast<popup_record&&>(rec));
    }

    void color_picker::paint_picker(painter& pnt, frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const rect r = m_picker.bounds();

        pnt.chiseled_panel(r, pal.popup_border_outer, pal.popup_border_inner, pal.popup_bg);

        const strips geometry = picker_strips(r);
        const rect sv = geometry.saturation_value;
        pnt.filled_rect({sv.x - 1.0f, sv.y - 1.0f, sv.w + 2.0f, sv.h + 2.0f}, pal.popup_border_outer);

        const math::color hue_c = hsv_to_rgb(m_hue, 1.0f, 1.0f, 1.0f);
        pnt.four_corner_gradient(sv, math::color::white(), hue_c, pal.text_shadow, pal.text_shadow);

        const float cur_x = sv.x + sv.w * m_sat;
        const float cur_y = sv.y + sv.h * (1.0f - m_val);
        pnt.outlined_rect({cur_x - 3.0f, cur_y - 3.0f, 6.0f, 6.0f}, pal.text_shadow);
        pnt.outlined_rect({cur_x - 2.0f, cur_y - 2.0f, 4.0f, 4.0f}, pal.picker_cursor);

        const rect hue_r = geometry.hue;
        pnt.filled_rect({hue_r.x - 1.0f, hue_r.y - 1.0f, hue_r.w + 2.0f, hue_r.h + 2.0f}, pal.popup_border_outer);
        {
            const float seg_h = hue_r.h / static_cast<float>(k_hue_strip_segments);
            for (int i = 0; i < k_hue_strip_segments; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(k_hue_strip_segments - 1);
                const auto c = hsv_to_rgb(t * 359.999f, 1.0f, 1.0f, 1.0f);
                pnt.filled_rect({hue_r.x, hue_r.y + seg_h * static_cast<float>(i), hue_r.w, seg_h + 1.0f}, c);
            }
        }

        math::color strip_cursor = pal.picker_cursor;
        strip_cursor.a = k_strip_cursor_alpha;
        const float hue_cy = hue_r.y + hue_r.h * (m_hue / 360.0f);
        pnt.filled_rect({hue_r.x - 1.0f, hue_cy - 2.0f, hue_r.w + 2.0f, 4.0f}, pal.text_shadow);
        pnt.filled_rect({hue_r.x, hue_cy - 1.0f, hue_r.w, 2.0f}, strip_cursor);

        if (!m_alpha_enabled) return;

        const rect alpha_r = geometry.alpha;
        pnt.filled_rect({alpha_r.x - 1.0f, alpha_r.y - 1.0f, alpha_r.w + 2.0f, alpha_r.h + 2.0f}, pal.popup_border_outer);
        pnt.checkerboard(alpha_r, 4, pal.checker_light, pal.checker_dark);

        const auto opaque = hsv_to_rgb(m_hue, m_sat, m_val, 1.0f);
        pnt.horizontal_gradient(alpha_r, math::color{opaque.r, opaque.g, opaque.b, 0}, math::color{opaque.r, opaque.g, opaque.b, 255});

        const float alpha_cx = alpha_r.x + alpha_r.w * m_alpha;
        pnt.filled_rect({alpha_cx - 2.0f, alpha_r.y - 1.0f, 4.0f, alpha_r.h + 2.0f}, pal.text_shadow);
        pnt.filled_rect({alpha_cx - 1.0f, alpha_r.y, 2.0f, alpha_r.h}, strip_cursor);
    }

    bool color_picker::input_picker(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        m_picker.set_bounds(compute_picker_bounds(ctx));

        const rect r = m_picker.bounds();
        const strips geometry = picker_strips(r);
        const rect sv = geometry.saturation_value;
        const rect hue_r = geometry.hue;
        const rect alpha_r = geometry.alpha;

        if (ctx.input().key_pressed(VK_LBUTTON)) {
            if (sv.contains(cursor))
                m_drag = drag::sv;
            else if (hue_r.contains(cursor))
                m_drag = drag::hue;
            else if (m_alpha_enabled && alpha_r.contains(cursor)) {
                m_drag = drag::alpha;
            }
        }

        if (m_drag != drag::none) {
            if (!ctx.input().key_down(VK_LBUTTON)) {
                m_drag = drag::none;

                if (m_commit_on_release) publish_color();
            } else {
                switch (m_drag) {
                    case drag::sv: {
                        const float u = dh::clamp((cursor.x - sv.x) / sv.w, 0.0f, 1.0f);
                        const float v = dh::clamp((cursor.y - sv.y) / sv.h, 0.0f, 1.0f);
                        m_sat = u;
                        m_val = 1.0f - v;
                        if (!m_commit_on_release) publish_color();
                        break;
                    }
                    case drag::hue: {
                        const float u = dh::clamp((cursor.y - hue_r.y) / hue_r.h, 0.0f, 1.0f);
                        m_hue = u * 360.0f;
                        if (!m_commit_on_release) publish_color();
                        break;
                    }
                    case drag::alpha: {
                        const float u = dh::clamp((cursor.x - alpha_r.x) / alpha_r.w, 0.0f, 1.0f);
                        m_alpha = u;
                        if (!m_commit_on_release) publish_color();
                        break;
                    }
                    case drag::none:
                        break;
                }
                ctx.consume_mouse();
            }
        }

        if (ctx.input().key_pressed(VK_LBUTTON)) ctx.consume_mouse();

        return m_drag != drag::none || r.contains(cursor);
    }

    void color_picker::paint_context(painter& pnt, frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const rect r = m_context.bounds();
        pnt.chiseled_panel(r, pal.popup_border_outer, pal.popup_border_inner, pal.popup_bg);

        const float content_x = r.x + 1.0f;
        const float content_y = r.y + list_popup_pad(ctx);
        const float content_w = r.w - 2.0f;

        const float text_inset = static_cast<float>(ctx.theme().metrics().popup_row_text_inset);
        const float item_h = static_cast<float>(ctx.theme().metrics().popup_row_h);
        for (int i = 0; i < k_ctx_items; ++i) {
            const rect item_r{content_x, content_y + item_h * static_cast<float>(i), content_w, item_h};
            const bool hov = (m_context_hover_idx == i);
            if (hov) {
                pnt.filled_rect(item_r, ctx.theme().accent_wash());
                pnt.filled_rect({item_r.x, item_r.y, static_cast<float>(ctx.theme().metrics().row_stripe_w), item_r.h}, ctx.theme().accent_value());
            }

            const float tx = item_r.x + text_inset;
            const float ty = pnt.text_center_y(item_r);
            const math::color tc = hov ? pal.text_primary : pal.text_secondary;
            pnt.text({tx, ty}, k_ctx_labels[static_cast<dh::size_t>(i)], tc);
        }
    }

    bool color_picker::input_context(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const rect bounds = m_context.bounds();
        m_context_hover_idx = popup_item_at(bounds, list_popup_pad(ctx), static_cast<float>(ctx.theme().metrics().popup_row_h), k_ctx_items, cursor);

        if (ctx.input().key_pressed(VK_LBUTTON) && m_context_hover_idx >= 0 && m_context_hover_idx < k_ctx_items) {
            switch (static_cast<context_action>(m_context_hover_idx)) {
                case context_action::copy:
                    g_clipboard = m_bound->peek();
                    break;
                case context_action::paste:

                    if (g_clipboard) {
                        m_bound->set(*g_clipboard);
                        sync_hsv_from_color();
                    }
                    break;
                case context_action::reset:
                    m_bound->set(m_reset_provider ? m_reset_provider() : m_initial_color);
                    sync_hsv_from_color();
                    break;

                case context_action::count:
                    break;
            }
            ctx.consume_mouse();
            m_context.close();
            return true;
        }
        return bounds.contains(cursor);
    }

    void color_picker::set_commit_on_release(bool enable) noexcept {
        m_commit_on_release = enable;
    }

    void color_picker::set_alpha_enabled(bool enabled) noexcept {
        m_alpha_enabled = enabled;
        if (!enabled) m_alpha = 1.0f;
    }

    void color_picker::set_reset_provider(dh::function<math::color()> provider) noexcept {
        m_reset_provider = dh::move(provider);
    }

    void color_picker::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();

        const float lty = ctx.pnt().text_center_y(m_rect);
        ctx.pnt().text_truncated({m_rect.x, lty}, dh::string_view{m_label.data(), m_label.size()}, m_rect.w * 0.75f, pal.text_label);

        const rect sw = compute_swatch_rect_ctx(ctx);
        ctx.pnt().filled_rect({sw.x - 1.0f, sw.y - 1.0f, sw.w + 2.0f, sw.h + 2.0f}, pal.border_outer);

        if (m_alpha_enabled) {
            ctx.pnt().checkerboard(sw, 3, pal.checker_light, pal.checker_dark);
        }

        ctx.pnt().filled_rect(sw, displayed_color());
    }

    bool color_picker::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const rect sw = compute_swatch_rect_ctx(ctx);
        const bool inside = m_rect.contains(cursor);

        const bool blocked = ctx.cursor_in_popup();

        if (inside && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }

        if (m_bound->peek() != m_last_synced) {
            sync_hsv_from_color();
        }

        if (sw.contains(cursor) && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            if (m_picker.is_open())
                close_picker();
            else
                open_picker(ctx);
            ctx.consume_mouse();
            return true;
        }
        if (sw.contains(cursor) && ctx.input().key_pressed(VK_RBUTTON) && !ctx.mouse_consumed()) {
            open_context(ctx);
            ctx.consume_mouse();
            return true;
        }
        return false;
    }

}
