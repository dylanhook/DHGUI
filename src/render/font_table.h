// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "render/font_atlas.h"

#include "core/dh/expected.h"
#include "core/dh/span.h"
#include "core/dh/string.h"

namespace dh::render {

    enum class font_id : dh::u8 {};

    // font 0 is the default when no font is specified.
    inline constexpr font_id k_body_font{0};

    enum class font_style : dh::u8 {
        none = 0,
        dropshadow = 1,
        outline = 2,
    };

    struct font_face final {
        constexpr font_face(dh::string_view face_label, font_config face_config, font_style face_style) noexcept : name(face_label), config(face_config), style(face_style) {}
        ~font_face() = default;
        constexpr font_face(const font_face&) noexcept = default;
        constexpr font_face& operator=(const font_face&) noexcept = default;
        constexpr font_face(font_face&&) noexcept = default;
        constexpr font_face& operator=(font_face&&) noexcept = default;

        dh::string_view name;
        font_config config;
        font_style style;
    };

    inline constexpr dh::size_t k_max_fonts = 16;

    enum class font_table_error : dh::u8 {
        empty,
        too_many_rows,
        unnamed_row,
        duplicate_name,
        empty_face,
        invalid_size,
    };

    [[nodiscard]] constexpr dh::string_view to_string(font_table_error error) noexcept {
        switch (error) {
            case font_table_error::empty: return "empty";
            case font_table_error::too_many_rows: return "too_many_rows";
            case font_table_error::unnamed_row: return "unnamed_row";
            case font_table_error::duplicate_name: return "duplicate_name";
            case font_table_error::empty_face: return "empty_face";
            case font_table_error::invalid_size: return "invalid_size";
        }
        return "unknown_font_table_error";
    }

    [[nodiscard]] constexpr dh::expected<void, font_table_error> validate_font_table(dh::span<const font_face> table) noexcept {
        if (table.empty()) return dh::unexpected{font_table_error::empty};
        if (table.size() > k_max_fonts) return dh::unexpected{font_table_error::too_many_rows};
        for (dh::size_t i = 0; i < table.size(); ++i) {
            if (table[i].name.empty()) return dh::unexpected{font_table_error::unnamed_row};
            if (table[i].config.face_name.empty()) return dh::unexpected{font_table_error::empty_face};
            if (!(table[i].config.em_size_px > 0.0f)) return dh::unexpected{font_table_error::invalid_size};
            for (dh::size_t j = 0; j < i; ++j) {
                if (table[j].name == table[i].name) return dh::unexpected{font_table_error::duplicate_name};
            }
        }
        return {};
    }

    enum class font_lookup_error : dh::u8 {
        name_unknown,
    };

    [[nodiscard]] constexpr dh::string_view to_string(font_lookup_error error) noexcept {
        switch (error) {
            case font_lookup_error::name_unknown: return "name_unknown";
        }
        return "unknown_font_lookup_error";
    }

    [[nodiscard]] constexpr dh::expected<font_id, font_lookup_error> find_font(dh::span<const font_face> table, dh::string_view name) noexcept {
        for (dh::size_t i = 0; i < table.size(); ++i) {
            if (table[i].name == name) return static_cast<font_id>(i);
        }
        return dh::unexpected{font_lookup_error::name_unknown};
    }

}
