// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "log/i_logger.h"

#include "core/dh/string.h"

namespace dh::demo_menu {

    class debug_logger final : public log::i_logger {
    public:
        debug_logger() noexcept = default;
        ~debug_logger() override = default;

        debug_logger(const debug_logger&) = delete;
        debug_logger& operator=(const debug_logger&) = delete;
        debug_logger(debug_logger&&) = delete;
        debug_logger& operator=(debug_logger&&) = delete;

        void log(log::level lvl, dh::string_view scope, dh::source_location loc, dh::string message) noexcept override;

        [[nodiscard]] dh::size_t complaints() const noexcept { return m_complaints; }

    private:
        dh::size_t m_complaints{0};
    };

}
