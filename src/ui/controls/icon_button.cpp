// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "icon_button.h"

#include "render/d3d11_renderer.h"
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
        constexpr float k_icon_h = 28.0f;
        constexpr float k_cell_w = 84.0f;

        constexpr float k_caption_gap = 0.0f;
        constexpr float k_caption_bottom_pad = 2.0f;
        constexpr float k_side_pad = 6.0f;

        constexpr float k_equal_area_side = 25.5f;
    }

    icon_button::icon_button(reactive::signal<int>& selection, int index, dh::string caption, glyph_fit fit, dh::function<icon_source()> source) noexcept : m_selection(&selection), m_index(index), m_caption(dh::move(caption)), m_fit(fit), m_source(dh::move(source)) {
        m_hover_t.snap(0.0f);
    }

    int icon_button::cell_height(const frame_ctx& ctx) noexcept {
        return static_cast<int>(k_icon_h + k_caption_gap + k_caption_bottom_pad) + ctx.pnt().text_line_h();
    }

    size icon_button::desired_size(const frame_ctx& ctx) const noexcept {
        return {static_cast<int>(k_cell_w), cell_height(ctx)};
    }

    void icon_button::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const bool sel = m_selection->peek_base() == m_index;
        const float t = m_hover_t.value();

        const icon_source src = m_source ? m_source() : icon_source{};
        if (src.srv != nullptr && src.width > 0.0f && src.height > 0.0f) {
            const float area_w = m_rect.w - k_side_pad * 2.0f;
            const float aspect = src.width / src.height;

            float draw_h = m_fit == glyph_fit::equal_area ? dh::min(k_equal_area_side / dh::sqrt(aspect), k_icon_h) : k_icon_h;
            float draw_w = draw_h * aspect;
            if (draw_w > area_w) {
                draw_w = area_w;
                draw_h = draw_w / aspect;
            }
            const float ix = m_rect.x + (m_rect.w - draw_w) * 0.5f;
            const float iy = m_rect.y + (k_icon_h - draw_h) * 0.5f;

            math::color tint = sel ? ctx.theme().accent_value() : (t > 0.0f ? pal.text_secondary : pal.text_muted);
            tint.a = static_cast<dh::u8>(static_cast<float>(tint.a) * ctx.pnt().alpha());
            ctx.pnt().renderer().add_image(src.srv, ix, iy, draw_w, draw_h, tint);
        }

        if (!m_caption.empty()) {
            const dh::string_view caption_view{m_caption.data(), m_caption.size()};

            const float avail = m_rect.w - k_side_pad * 2.0f;
            const float cw = ctx.pnt().measure_text_w(caption_view);
            const float cx = (cw <= avail) ? m_rect.x + (m_rect.w - cw) * 0.5f : m_rect.x + k_side_pad;
            const float cy = m_rect.y + k_icon_h + k_caption_gap;
            const math::color cc = sel ? pal.text_primary : (t > 0.0f ? pal.text_secondary : pal.text_muted);
            ctx.pnt().text_truncated({cx, cy}, caption_view, avail, cc);
        }
    }

    bool icon_button::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const bool inside = m_rect.contains(cursor);
        const bool blocked = ctx.cursor_in_popup();

        if (inside && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }
        m_hover_t.set_target((inside && !blocked) ? 1.0f : 0.0f, ctx.theme().times().hover);

        if (inside && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            m_selection->set(m_index);
            ctx.consume_mouse();
            return true;
        }
        return false;
    }

}
