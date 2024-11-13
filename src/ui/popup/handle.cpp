// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "handle.h"

#include "ui/core/frame_ctx.h"
#include "ui/core/control.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

namespace dh::ui {

    popup_handle::popup_handle(control& owner, popup_slot slot) noexcept : m_owner(&owner), m_key(key_for(owner.id(), slot)), m_input(this, [](void* self) noexcept { static_cast<popup_handle*>(self)->dismiss(); }) {}

    popup_handle::~popup_handle() {
        close();
    }

    void popup_handle::open(frame_ctx& ctx, popup_record rec) noexcept {
        close();
        if (!m_input.attach(*m_owner)) return;
        m_layer = &ctx.popups();
        m_bounds = rec.bounds;
        rec.key = m_key;
        m_layer->open(static_cast<popup_record&&>(rec));
    }

    void popup_handle::close() noexcept {
        m_input.reset();
        if (m_layer) m_layer->close(m_key);
        m_layer = nullptr;
    }

    void popup_handle::dismiss() noexcept {
        m_input.reset();
        popup_layer* layer = m_layer;
        m_layer = nullptr;
        if (layer != nullptr) layer->dismiss(m_key);
    }

    bool popup_handle::is_open() const noexcept {
        return m_layer != nullptr && m_layer->is_open(m_key);
    }

    void popup_handle::set_bounds(rect r) noexcept {
        m_bounds = r;
        if (m_layer) m_layer->set_bounds(m_key, r);
    }

    int popup_item_at(rect bounds, float pad, float item_h, int count, point cursor) noexcept {
        if (count <= 0 || item_h <= 0.0f) return -1;
        if (cursor.x < bounds.x || cursor.x >= bounds.right()) return -1;

        const float top = bounds.y + pad;
        const float bot = top + item_h * static_cast<float>(count);
        if (cursor.y < top || cursor.y >= bot) return -1;

        const int idx = static_cast<int>((cursor.y - top) / item_h);
        return (idx >= 0 && idx < count) ? idx : -1;
    }

    float list_popup_pad(const frame_ctx& ctx) noexcept {
        return static_cast<float>(ctx.theme().metrics().popup_pad);
    }

}
