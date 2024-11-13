// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/string.h"

namespace dh::ui::controls {

    inline constexpr dh::size_t k_popup_search_min_options = 14;

    [[nodiscard]] inline bool option_contains_ci(dh::string_view hay, dh::string_view needle) noexcept {
        if (needle.size() > hay.size()) return false;
        const auto lower = [](char c) noexcept -> char { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; };
        for (dh::size_t i = 0; i + needle.size() <= hay.size(); ++i) {
            dh::size_t j = 0;
            while (j < needle.size() && lower(hay[i + j]) == lower(needle[j])) ++j;
            if (j == needle.size()) return true;
        }
        return false;
    }

}
