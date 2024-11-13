// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/optional.h"
#include "math/color.h"
#include "ui/core/types.h"
#include "ui/theme/theme.h"

namespace dh::ui::menu {

    struct view_state final {
    public:
        view_state() noexcept = default;
        ~view_state() = default;
        view_state(const view_state&) = default;
        view_state& operator=(const view_state&) = default;
        view_state(view_state&&) = default;
        view_state& operator=(view_state&&) = default;

        bool open{false};
        dh::optional<rect> bounds{};
        math::color accent{};
        float alpha{0.0f};
        theme::palette colors{};
    };

}
