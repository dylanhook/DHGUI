// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "theme.h"

namespace dh::ui::theme {

    theme::theme(reactive::signal<math::color>& accent, font_roles fonts) noexcept : m_fonts(fonts), m_accent(&accent) {}

    math::color theme::accent_value() const noexcept {
        return m_accent->peek();
    }

    math::color theme::accent_wash() const noexcept {
        constexpr dh::u8 k_wash_alpha = 36;
        const auto c = m_accent->peek();
        return {c.r, c.g, c.b, k_wash_alpha};
    }

}
