// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "hud_list.h"

#include "math/color.h"
#include "ui/core/frame_ctx.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include "core/dh/algorithm.h"

namespace dh::ui::overlay::hud_list {

    namespace {

        constexpr float k_accent_h = 2.0f;
        constexpr float k_header_pad = 4.0f;

        constexpr float k_header_opacity = 0.90f;

        constexpr float k_rows_margin = 4.0f;
        constexpr float k_rows_top = k_rows_margin + painter::k_text_center_bias;
        constexpr float k_rows_bot = k_rows_margin - painter::k_text_center_bias;

        [[nodiscard]] float header_h(const frame_ctx& ctx) noexcept { return row_h(ctx) + k_header_pad * 2.0f; }

    }

    render::font_id font(const frame_ctx& ctx) noexcept {
        return ctx.theme().fonts().readout;
    }

    float row_h(const frame_ctx& ctx) noexcept {
        return static_cast<float>(ctx.pnt().text_line_h(font(ctx)));
    }

    size panel_size(const frame_ctx& ctx, float content_w, dh::size_t rows) noexcept {
        const float w = dh::max(k_min_w, content_w + k_side_pad * 2.0f);
        const float h = k_accent_h + header_h(ctx) + k_rows_top + row_h(ctx) * static_cast<float>(rows) + k_rows_bot;
        return {static_cast<int>(w + 0.5f), static_cast<int>(h + 0.5f)};
    }

    float draw_chrome(frame_ctx& ctx, rect panel, dh::string_view title) noexcept {
        painter& pnt = ctx.pnt();
        const auto& pal = ctx.theme().colors();

        pnt.filled_rect({panel.x, panel.y, panel.w, k_accent_h}, ctx.theme().accent_value());

        const rect header{panel.x, panel.y + k_accent_h, panel.w, header_h(ctx)};
        pnt.filled_rect(header, math::scaled_alpha(pal.form_bg, k_header_opacity));

        const render::font_id face = font(ctx);
        const float title_x = header.x + (header.w - pnt.measure_text_w(face, title)) * 0.5f;
        pnt.text(face, {title_x, pnt.text_center_y(face, header)}, title, pal.text_primary);

        return header.bottom() + k_rows_top;
    }

}
