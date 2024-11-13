// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh {

    namespace detail {
        struct in_place_tag {};
    }

    struct in_place_t final {
        explicit constexpr in_place_t(detail::in_place_tag) noexcept {}
    };

    inline constexpr in_place_t in_place{detail::in_place_tag{}};

}
