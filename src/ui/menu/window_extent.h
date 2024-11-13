// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/algorithm.h"
#include "ui/theme/theme.h"

namespace dh::ui::menu {

    inline constexpr int k_window_min_width = theme::sizes{}.window_min_w;
    inline constexpr int k_window_min_height = theme::sizes{}.window_min_h;

    inline constexpr int k_window_max_extent = 65536;

    [[nodiscard]] constexpr bool is_window_width(const int& width) noexcept {
        return width >= k_window_min_width && width <= k_window_max_extent;
    }

    [[nodiscard]] constexpr bool is_window_height(const int& height) noexcept {
        return height >= k_window_min_height && height <= k_window_max_extent;
    }

    [[nodiscard]] constexpr int drawn_window_extent(int chosen, int minimum, int viewport) noexcept {
        return dh::max(minimum, dh::min(chosen, viewport));
    }

}
