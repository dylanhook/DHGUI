// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh::ui::animation::easing {

    [[nodiscard]] constexpr float cubic_out(float t) noexcept {
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }

}
