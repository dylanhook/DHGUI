// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "debug_logger.h"

#include "core/dh/format.h"

namespace dh::demo_menu {

    void debug_logger::log(log::level lvl, dh::string_view scope, dh::source_location loc, dh::string message) noexcept {
        if (lvl >= log::level::warn) ++m_complaints;
        dh::string line = dh::format("[{}] [{}] {}:{} {}\n", log::to_short(lvl), scope, loc.file_id(), loc.line(), message);
        ::OutputDebugStringA(line.c_str());
        const HANDLE err = ::GetStdHandle(STD_ERROR_HANDLE);
        if (err != nullptr && err != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            static_cast<void>(::WriteFile(err, line.data(), static_cast<DWORD>(line.size()), &written, nullptr));
        }
    }

}
