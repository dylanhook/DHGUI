// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "painter.h"

#include "ui/theme/theme.h"

#include "math/constants.h"

#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"

namespace dh::ui {

    void painter::filled_rect(rect r, math::color c) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        m_renderer->add_filled_rect(r.x, r.y, r.w, r.h, c);
    }

    void painter::outlined_rect(rect r, math::color c, float thickness) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        m_renderer->add_outlined_rect(r.x, r.y, r.w, r.h, c, thickness);
    }

    namespace {

        constexpr int k_ring_segments = 48;

    }

    void painter::progress_ring(point center, float radius, float thickness, float fraction, math::color track, math::color fill) noexcept {
        if (radius <= 0.0f || thickness <= 0.0f) return;
        const float outer = radius;
        const float inner = radius - thickness > 0.0f ? radius - thickness : 0.0f;
        const float swept = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);

        constexpr float k_start = -math::k_two_pi * 0.25f;
        const auto quad = [&](float a0, float a1, math::color c) noexcept {
            const float c0 = dh::cos(a0), s0 = dh::sin(a0);
            const float c1 = dh::cos(a1), s1 = dh::sin(a1);
            const float ox0 = center.x + c0 * outer, oy0 = center.y + s0 * outer;
            const float ox1 = center.x + c1 * outer, oy1 = center.y + s1 * outer;
            const float ix0 = center.x + c0 * inner, iy0 = center.y + s0 * inner;
            const float ix1 = center.x + c1 * inner, iy1 = center.y + s1 * inner;
            m_renderer->add_filled_triangle_aa(ox0, oy0, ox1, oy1, ix0, iy0, c);
            m_renderer->add_filled_triangle_aa(ix0, iy0, ox1, oy1, ix1, iy1, c);
        };
        const auto angle = [](int i) noexcept { return k_start + math::k_two_pi * static_cast<float>(i) / static_cast<float>(k_ring_segments); };

        for (int i = 0; i < k_ring_segments; ++i)
            quad(angle(i), angle(i + 1), track);

        const float end = k_start + math::k_two_pi * swept;
        const float exact = swept * static_cast<float>(k_ring_segments);
        const int whole = static_cast<int>(exact);
        for (int i = 0; i < whole; ++i)
            quad(angle(i), angle(i + 1), fill);
        if (whole < k_ring_segments && end > angle(whole)) quad(angle(whole), end, fill);
    }

    void painter::filled_capsule_v(rect r, math::color c) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;

        constexpr float k_min_cap_radius = 1.5f;
        const float radius = dh::min(r.w * 0.5f, r.h * 0.5f);
        if (radius < k_min_cap_radius) {
            filled_rect(r, c);
            return;
        }

        filled_rect({r.x, r.y + radius, r.w, r.h - radius * 2.0f}, c);

        const int rows = static_cast<int>(radius);
        for (int i = 0; i < rows; ++i) {
            const float dy = radius - (static_cast<float>(i) + 0.5f);
            const float half = dh::sqrt(dh::max(0.0f, radius * radius - dy * dy));
            const float w = half * 2.0f;
            const float x = r.x + (r.w - w) * 0.5f;
            filled_rect({x, r.y + static_cast<float>(i), w, 1.0f}, c);
            filled_rect({x, r.bottom() - static_cast<float>(i) - 1.0f, w, 1.0f}, c);
        }
    }

    void painter::form_frame(rect inner, const theme::palette& pal, const theme::sizes& metrics) noexcept {
        const float bezel = static_cast<float>(metrics.form_bezel);
        const rect outer{inner.x - bezel, inner.y - bezel, inner.w + bezel * 2.0f, inner.h + bezel * 2.0f};
        const float hair = hairline();
        filled_rect(outer, pal.form_outer);
        outlined_rect(outer.inset(hair), pal.form_rim, hair);
        outlined_rect(outer.inset(hair * 2.0f), pal.border_inner, hair * 2.0f);
        outlined_rect(outer.inset(hair * 4.0f), pal.form_rim, hair);
        filled_rect(inner, pal.form_bg);
    }

    void painter::chiseled_panel(rect r, math::color outer, math::color inner, math::color fill) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;

        filled_rect(r, outer);
        const float hair = hairline();
        const rect body = r.inset(hair);
        if (fill.a > 0) {
            filled_rect(body, fill);
        }
        if (inner.a > 0) {
            filled_rect({body.x, body.y, body.w, hair}, inner);
        }
    }

    void painter::chevron_down(point center, int base_w, math::color color, math::color shadow) noexcept {
        const int steps = (base_w + 1) / 2;
        for (int i = 0; i < steps; ++i) {
            const float w = static_cast<float>(base_w - 2 * i);
            if (w <= 0.0f) break;
            const float x = center.x - w * 0.5f;
            const float y = center.y - static_cast<float>(steps) * 0.5f + static_cast<float>(i);

            filled_rect({x, y + 1.0f, w, 1.0f}, shadow);
            filled_rect({x, y, w, 1.0f}, color);
        }
    }

    void painter::chevron_right(point center, int base_h, math::color color, math::color shadow) noexcept {
        const int steps = (base_h + 1) / 2;
        for (int i = 0; i < steps; ++i) {
            const float h = static_cast<float>(base_h - 2 * i);
            if (h <= 0.0f) break;
            const float y = center.y - h * 0.5f;
            const float x = center.x - static_cast<float>(steps) * 0.5f + static_cast<float>(i);
            filled_rect({x + 1.0f, y, 1.0f, h}, shadow);
            filled_rect({x, y, 1.0f, h}, color);
        }
    }

    void painter::draw_cursor(point tip, math::color fill, math::color outline) noexcept {
        constexpr int k_h = 19;
        constexpr int k_w = 12;
        constexpr dh::u8 k_cursor[k_h][k_w] = {
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
            {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
            {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
            {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
            {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
            {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
            {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0},
            {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0},
            {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1},
            {1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 0},
            {1, 2, 2, 2, 1, 2, 2, 2, 1, 0, 0, 0},
            {1, 2, 2, 1, 0, 1, 2, 2, 2, 1, 0, 0},
            {1, 2, 1, 0, 0, 1, 2, 2, 2, 1, 0, 0},
            {1, 1, 0, 0, 0, 0, 1, 2, 2, 2, 1, 0},
            {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0},
        };

        const float px = hairline();

        for (int y = 0; y < k_h; ++y) {
            int x = 0;
            while (x < k_w) {
                const dh::u8 v = k_cursor[y][x];
                if (v == 0) {
                    ++x;
                    continue;
                }
                int run_x = x;
                while (run_x < k_w && k_cursor[y][run_x] == v)
                    ++run_x;
                const float w = static_cast<float>(run_x - x) * px;
                const math::color c = (v == 1) ? outline : fill;
                filled_rect({tip.x + static_cast<float>(x) * px, tip.y + static_cast<float>(y) * px, w, px}, c);
                x = run_x;
            }
        }
    }

    void painter::group_frame(rect r, float title_left, float title_right, const theme::palette& pal) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        const float hair = hairline();
        filled_rect(r.inset(hair * 2.0f), pal.group_bg);

        const auto border = [&](rect edge, math::color color) noexcept {
            if (edge.w <= 0.0f || edge.h <= 0.0f) return;
            const float thickness = dh::min(hair, dh::min(edge.w, edge.h) * 0.5f);
            const float gap_left = dh::clamp(title_left, edge.x, edge.right());
            const float gap_right = dh::clamp(title_right, gap_left, edge.right());
            filled_rect({edge.x, edge.y, gap_left - edge.x, thickness}, color);
            filled_rect({gap_right, edge.y, edge.right() - gap_right, thickness}, color);
            filled_rect({edge.x, edge.y + thickness, thickness, edge.h - thickness * 2.0f}, color);
            filled_rect({edge.right() - thickness, edge.y + thickness, thickness, edge.h - thickness * 2.0f}, color);
            filled_rect({edge.x, edge.bottom() - thickness, edge.w, thickness}, color);
        };
        border(r, pal.group_border_outer);
        border(r.inset(hair), pal.group_border_inner);
    }

    void painter::pill_button(rect r, const theme::palette& pal, chip_state state, float hover_t) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        const math::color fill = state == chip_state::active ? pal.list_row_hover : math::lerp(pal.group_bg, pal.control_top_hover, hover_t);
        filled_rect(r, pal.border_outer);
        filled_rect(r.inset(hairline()), fill);
}

    void painter::vertical_gradient(rect r, math::color top, math::color bot) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        m_renderer->add_gradient_rect(r.x, r.y, r.w, r.h, top, top, bot, bot);
    }

    void painter::horizontal_gradient(rect r, math::color left, math::color right) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        m_renderer->add_gradient_rect(r.x, r.y, r.w, r.h, left, right, left, right);
    }

    void painter::four_corner_gradient(rect r, math::color tl, math::color tr, math::color bl, math::color br) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f) return;
        m_renderer->add_gradient_rect(r.x, r.y, r.w, r.h, tl, tr, bl, br);
    }

    void painter::checkerboard(rect r, int cell_size, math::color base, math::color alternate) noexcept {
        if (r.w <= 0.0f || r.h <= 0.0f || cell_size <= 0) return;

        m_renderer->add_filled_rect(r.x, r.y, r.w, r.h, base);
        m_renderer->add_checker_rect(r.x, r.y, r.w, r.h, static_cast<float>(cell_size), alternate);
        m_renderer->force_new_segment();
    }

    void painter::text(point p, dh::string_view s, math::color c) noexcept {
        if (s.empty()) return;
        m_renderer->add_text(p.x, p.y, s, c);
    }

    void painter::text(render::font_id font, point p, dh::string_view s, math::color c) noexcept {
        if (s.empty()) return;
        m_renderer->add_text(font, p.x, p.y, s, c);
    }

    float painter::text_center_y(rect r) const noexcept {
        return r.y + (r.h - static_cast<float>(text_line_h())) * 0.5f - k_text_center_bias;
    }

    float painter::text_center_y(render::font_id font, rect r) const noexcept {
        return r.y + (r.h - static_cast<float>(text_line_h(font))) * 0.5f - k_text_center_bias;
    }

    void painter::text_centered(rect r, dh::string_view s, math::color c) noexcept {
        if (s.empty()) return;
        const float x = r.x + (r.w - measure_text_w(s)) * 0.5f;
        m_renderer->add_text(x, text_center_y(r), s, c);
    }

    void painter::text_truncated(point p, dh::string_view s, float max_w, math::color c) noexcept {
        m_renderer->add_text_truncated(p.x, p.y, s, max_w, c);
    }

    float painter::measure_text_w(dh::string_view s) const noexcept {
        return m_renderer->measure_text_width(s);
    }

    float painter::measure_text_w(render::font_id font, dh::string_view s) const noexcept {
        return m_renderer->measure_text_width(font, s);
    }

    dh::expected<render::text_caret, render::atlas_failure> painter::trailing_text_caret(dh::string_view text) const noexcept {
        return m_renderer->trailing_text_caret(render::k_body_font, text);
    }

    int painter::text_line_h() const noexcept {
        return m_renderer->text_line_height();
    }

    int painter::text_line_h(render::font_id font) const noexcept {
        return m_renderer->text_line_height(font);
    }

    void painter::push_clip(rect r) noexcept {
        m_renderer->push_clip_rect(r.x, r.y, r.w, r.h);
    }

    void painter::pop_clip() noexcept {
        m_renderer->pop_clip_rect();
    }

    void painter::set_alpha(float a) noexcept {
        m_alpha = dh::clamp(a, 0.0f, 1.0f);
        m_renderer->set_alpha_multiplier(m_alpha);
    }

}
