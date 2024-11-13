// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "i_logger.h"

#include "core/dh/source_location.h"

#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh::log {

    void set_active(i_logger* logger) noexcept;

    [[nodiscard]] i_logger& active() noexcept;

    template <typename... Args> void debug(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
        active().debug(fmt, dh::forward<Args>(args)...);
    }

    template <typename... Args> void info(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
        active().info(fmt, dh::forward<Args>(args)...);
    }

    template <typename... Args> void warn(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
        active().warn(fmt, dh::forward<Args>(args)...);
    }

    template <typename... Args> void error(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
        active().error(fmt, dh::forward<Args>(args)...);
    }

    template <typename... Args> void fatal(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
        active().fatal(fmt, dh::forward<Args>(args)...);
    }

}
