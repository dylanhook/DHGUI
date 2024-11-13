// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "i_logger.h"
#include "level.h"

#include "core/dh/source_location.h"
#include "core/dh/string.h"

#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh::log {

    class scoped_logger final {
    public:
        scoped_logger(i_logger& parent, dh::string source) noexcept : m_parent(&parent), m_scope(dh::move(source)) {}

        scoped_logger(const scoped_logger&) = default;
        scoped_logger(scoped_logger&&) noexcept = default;
        scoped_logger& operator=(const scoped_logger&) = default;
        scoped_logger& operator=(scoped_logger&&) noexcept = default;
        ~scoped_logger() = default;

        [[nodiscard]] dh::string_view scope() const noexcept { return m_scope; }

        [[nodiscard]] i_logger& parent() const noexcept { return *m_parent; }

        template <typename... Args> void debug(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) const noexcept {
            emit<level::debug>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void info(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) const noexcept {
            emit<level::info>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void warn(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) const noexcept {
            emit<level::warn>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void error(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) const noexcept {
            emit<level::error>(fmt, dh::forward<Args>(args)...);
        }

        template <typename... Args> void fatal(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) const noexcept {
            emit<level::fatal>(fmt, dh::forward<Args>(args)...);
        }

    private:
        template <level Lvl, typename... Args> void emit(detail::fmt_with_loc<dh::type_identity_t<Args>...> fmt, Args&&... args) const noexcept {
            detail::emit_to<Lvl>(*m_parent, m_scope, fmt, dh::forward<Args>(args)...);
        }

        i_logger* m_parent;
        dh::string m_scope;
    };

    inline scoped_logger i_logger::scoped(dh::string source) noexcept {
        return scoped_logger{*this, dh::move(source)};
    }

}
