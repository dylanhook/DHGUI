// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/core/types.h"

namespace dh::ui {
    class frame_ctx;
}

namespace dh::ui::overlay {

    enum class menu_state : dh::u8 { closed, open };

    class i_overlay {
    public:
        i_overlay() = default;
        virtual ~i_overlay() = default;

        i_overlay(const i_overlay&) = delete;
        i_overlay& operator=(const i_overlay&) = delete;
        i_overlay(i_overlay&&) = delete;
        i_overlay& operator=(i_overlay&&) = delete;

        [[nodiscard]] virtual bool paint(frame_ctx& ctx, menu_state menu) noexcept = 0;

        virtual void handle_input(frame_ctx& ctx, menu_state menu) noexcept = 0;

        [[nodiscard]] virtual bool contains(point position) const noexcept = 0;
    };

}
