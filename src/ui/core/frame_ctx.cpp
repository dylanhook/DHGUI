// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "frame_ctx.h"

#include "ui/painter/painter.h"
#include "ui/popup/layer.h"

namespace dh::ui {

    point frame_ctx::cursor() const noexcept {
        const auto p = m_input->mouse_pos();
        // input arrives in device pixels, controls use authored pixels.
        const float scale = m_painter->renderer().dpi_scale();
        return {p.x / scale, p.y / scale};
    }

    bool frame_ctx::cursor_in_popup() const noexcept {
        return m_popups->cursor_inside_any(cursor());
    }

    bool frame_ctx::popup_blocks_scroll() const noexcept {
        return m_popups->blocks_scroll();
    }

}
