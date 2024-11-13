// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/string.h"

#include "core/dh/memory.h"

namespace dh::io {

    enum class io_error : unsigned char {
        open_failed,
        write_failed,
        flush_failed,
        already_closed,
        size_query_failed,
        read_failed,
        file_changed,
        too_large,
        range_invalid,
    };

    [[nodiscard]] constexpr dh::string_view to_string(io_error e) noexcept {
        switch (e) {
            case io_error::open_failed:
                return "open_failed";
            case io_error::write_failed:
                return "write_failed";
            case io_error::flush_failed:
                return "flush_failed";
            case io_error::already_closed:
                return "already_closed";
            case io_error::size_query_failed:
                return "size_query_failed";
            case io_error::read_failed:
                return "read_failed";
            case io_error::file_changed:
                return "file_changed";
            case io_error::too_large:
                return "too_large";
            case io_error::range_invalid:
                return "range_invalid";
        }
        return "unknown";
    }

    struct io_failure final {
        io_error step;
        dh::u32 os_error;
    };

}
