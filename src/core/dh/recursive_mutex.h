// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include <Windows.h>

#include "core/dh/expected.h"
#include "core/dh/string.h"

namespace dh {

    enum class recursive_mutex_error { system_refused };

    [[nodiscard]] constexpr dh::string_view to_string(recursive_mutex_error error) noexcept {
        switch (error) {
            case recursive_mutex_error::system_refused:
                return "system_refused";
        }
        return "unknown";
    }

    class recursive_mutex final {
    public:
        recursive_mutex() noexcept = default;

        ~recursive_mutex() { close(); }

        recursive_mutex(const recursive_mutex&) = delete;
        recursive_mutex& operator=(const recursive_mutex&) = delete;
        recursive_mutex(recursive_mutex&&) = delete;
        recursive_mutex& operator=(recursive_mutex&&) = delete;

        [[nodiscard]] dh::expected<void, recursive_mutex_error> initialize() noexcept {
            if (m_ready) return {};
            if (::InitializeCriticalSectionEx(&m_native, 0, 0) == FALSE) {
                return dh::unexpected{recursive_mutex_error::system_refused};
            }
            m_ready = true;
            return {};
        }

        void close() noexcept {
            if (!m_ready) return;
            ::DeleteCriticalSection(&m_native);
            m_ready = false;
        }

        void lock() noexcept { ::EnterCriticalSection(&m_native); }

        void unlock() noexcept { ::LeaveCriticalSection(&m_native); }

    private:
        CRITICAL_SECTION m_native{};
        bool m_ready{false};
    };

}
