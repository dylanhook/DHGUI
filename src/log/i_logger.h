// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "level.h"

#include "core/dh/format.h"
#include "core/dh/source_location.h"
#include "core/dh/string.h"

#include "core/dh/concepts.h"
#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh::log {

    class scoped_logger;
    class i_logger;

    namespace detail {

        template <typename... Args> struct fmt_with_loc {
            dh::string_view fmt;
            dh::source_location loc;

            template <typename Str>
                requires dh::convertible_to<const Str&, dh::string_view>
            consteval fmt_with_loc(const Str& f, dh::source_location l = dh::source_location::current()) : fmt(f), loc(l) {}
        };

        template <level Lvl, typename... Args>
        inline void emit_to(::dh::log::i_logger& parent, dh::string_view scope, const fmt_with_loc<Args...>& fmt_loc, Args&&... args) noexcept;

    }

    class i_logger {
    public:
        i_logger() = default;
        virtual ~i_logger() = default;

        i_logger(const i_logger&) = delete;
        i_logger& operator=(const i_logger&) = delete;
        i_logger(i_logger&&) = delete;
        i_logger& operator=(i_logger&&) = delete;

        virtual void log(level lvl, dh::string_view scope, dh::source_location loc, dh::string message) noexcept = 0;

        [[nodiscard]] scoped_logger scoped(dh::string source) noexcept;

        template <typename... Args> void debug(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
            emit<level::debug>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void info(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
            emit<level::info>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void warn(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
            emit<level::warn>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void error(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
            emit<level::error>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void fatal(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
            emit<level::fatal>(fmt, dh::forward<Args>(args)...);
        }

    private:
        template <level Lvl, typename... Args> void emit(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) noexcept {
            detail::emit_to<Lvl>(*this, dh::string_view{}, fmt, dh::forward<Args>(args)...);
        }
    };

    namespace detail {

        template <level Lvl, typename... Args>
        inline void emit_to(::dh::log::i_logger& parent, dh::string_view scope, const fmt_with_loc<Args...>& fmt_loc, Args&&... args) noexcept {
            if constexpr (Lvl >= compile_min_level) {
                parent.log(Lvl, scope, fmt_loc.loc, dh::format(fmt_loc.fmt, dh::forward<Args>(args)...));
            }
        }

    }

}
