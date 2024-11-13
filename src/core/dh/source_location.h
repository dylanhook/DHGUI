// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/fnv1a.h"

namespace dh {

    class source_location final {
    public:
        [[nodiscard]] static consteval source_location current(const char* file = __builtin_FILE(), dh::u32 line = __builtin_LINE(), dh::u32 column = __builtin_COLUMN()) noexcept {
            source_location loc;
            if (file != nullptr && *file != '\0') {
                dh::fnv1a identity;
                for (const char* cursor = file; *cursor != '\0'; ++cursor) {
                    const auto value = static_cast<dh::u8>(*cursor == '\\' ? '/' : *cursor);
                    identity.combine(value);
                }
                loc.m_file_id = identity.value();
            }
            loc.m_line = line;
            loc.m_column = column;
            return loc;
        }

        constexpr source_location() noexcept = default;
        constexpr source_location(const source_location&) noexcept = default;
        constexpr source_location(source_location&&) noexcept = default;
        constexpr source_location& operator=(const source_location&) noexcept = default;
        constexpr source_location& operator=(source_location&&) noexcept = default;
        ~source_location() = default;

        [[nodiscard]] constexpr dh::u64 file_id() const noexcept { return m_file_id; }

        [[nodiscard]] constexpr dh::u32 line() const noexcept { return m_line; }

        [[nodiscard]] constexpr dh::u32 column() const noexcept { return m_column; }

    private:
        dh::u64 m_file_id{0};
        dh::u32 m_line{0};
        dh::u32 m_column{0};
    };

}
