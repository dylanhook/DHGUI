// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/string.h"
#include "render/font_table.h"
#include "ui/core/types.h"

namespace dh::ui {
    class frame_ctx;
    class painter;
}

namespace dh::ui::overlay::hud_list {

    [[nodiscard]] render::font_id font(const frame_ctx& ctx) noexcept;

    inline constexpr float k_side_pad = 6.0f;

    inline constexpr float k_min_w = 120.0f;

    inline constexpr float k_viewport_margin = 0.0f;

    [[nodiscard]] float row_h(const frame_ctx& ctx) noexcept;

    [[nodiscard]] size panel_size(const frame_ctx& ctx, float content_w, dh::size_t rows) noexcept;

    [[nodiscard]] float draw_chrome(frame_ctx& ctx, rect panel, dh::string_view title) noexcept;

}
