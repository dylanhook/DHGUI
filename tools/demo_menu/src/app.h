// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/string.h"

namespace dh::demo_menu {

    [[nodiscard]] int run_interactive() noexcept;
    [[nodiscard]] int run_capture(dh::wstring_view directory) noexcept;

}
