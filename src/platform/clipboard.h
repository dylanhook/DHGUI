// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/string.h"

namespace dh::platform::clipboard {

    enum class clipboard_error {
        no_owner_window,
        open_failed,
        empty,
        alloc_failed,
        set_failed,
        convert_failed,
        unterminated,
        too_large,
    };

    [[nodiscard]] dh::string_view to_string(clipboard_error e) noexcept;

    [[nodiscard]] dh::expected<void, clipboard_error> set_text(dh::string_view text, void* owner_window) noexcept;

    [[nodiscard]] dh::expected<dh::string, clipboard_error> get_text(dh::size_t max_utf8_bytes) noexcept;

    [[nodiscard]] dh::expected<dh::string, clipboard_error> get_text_now(dh::size_t max_utf8_bytes) noexcept;

}
