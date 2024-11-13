// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/pch_contract.h"
#include "core/dh/string.h"

namespace dh::log {

    enum class level : unsigned char {
        debug = 0,
        info = 1,
        warn = 2,
        error = 3,
        fatal = 4,
        off = 5,
    };

#if DH_LOG_COMPILED_OUT
    inline constexpr level compile_min_level = level::off;
#else
    inline constexpr level compile_min_level = level::info;
#endif

    [[nodiscard]] constexpr dh::string_view to_short(level lvl) noexcept {
        switch (lvl) {
            case level::debug:
                return "DBG";
            case level::info:
                return "INF";
            case level::warn:
                return "WRN";
            case level::error:
                return "ERR";
            case level::fatal:
                return "FTL";
            case level::off:
                return "";
        }
        return "???";
    }

}
