// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "i_logger.h"

#include "core/dh/source_location.h"
#include "core/dh/string.h"

namespace dh::log {

    class null_logger final : public i_logger {
    public:
        constexpr null_logger() noexcept = default;

        [[nodiscard]] static null_logger& instance() noexcept;

        void log(level, dh::string_view, dh::source_location, dh::string) noexcept override {}
    };

    namespace detail {

        inline constinit null_logger g_null_logger{};
    }

    inline null_logger& null_logger::instance() noexcept {
        return detail::g_null_logger;
    }

}
