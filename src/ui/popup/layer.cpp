// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "layer.h"

#include "ui/core/frame_ctx.h"
#include "ui/painter/painter.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui {

    dh::size_t popup_layer::find_index(control_id key) const noexcept {
        for (dh::size_t i = 0; i < m_records.size(); ++i) {
            if (m_records[i].key == key) return i;
        }
        return m_records.size();
    }

    void popup_layer::snapshot_keys() noexcept {
        // callbacks can close popups during this pass, so walk keys instead of vector entries.
        m_pass_keys.clear();
        for (const auto& rec : m_records) {
            m_pass_keys.push_back(rec.key);
        }
    }

    void popup_layer::open(popup_record rec) noexcept {
        if (!rec.key) return;

        const auto idx = find_index(rec.key);
        if (idx < m_records.size()) {
            m_records[idx] = dh::move(rec);
        } else {
            m_records.push_back(dh::move(rec));
        }

        dh::sort_by(m_records.begin(), m_records.end(), [](const popup_record& a, const popup_record& b) noexcept { return a.z_band < b.z_band; });
    }

    void popup_layer::close(control_id key) noexcept {
        const auto idx = find_index(key);
        if (idx >= m_records.size()) return;
        m_records.erase(m_records.begin() + static_cast<dh::ptrdiff_t>(idx));
    }

    void popup_layer::dismiss(control_id key) noexcept {
        const auto idx = find_index(key);
        if (idx >= m_records.size()) return;
        auto on_dismissed = dh::move(m_records[idx].on_dismissed);
        m_records.erase(m_records.begin() + static_cast<dh::ptrdiff_t>(idx));
        if (on_dismissed) on_dismissed();
    }

    void popup_layer::close_all() noexcept {
        // callbacks can open new popups, only close the ones we started with.
        auto records = dh::move(m_records);
        m_records.clear();
        for (auto& rec : records) {
            if (rec.on_dismissed) {
                rec.on_dismissed();
            }
        }
    }

    bool popup_layer::is_open(control_id key) const noexcept {
        return find_index(key) < m_records.size();
    }

    bool popup_layer::cursor_inside_any(point cursor) const noexcept {
        for (const auto& rec : m_records) {
            if (rec.bounds.contains(cursor)) return true;
        }
        return false;
    }

    bool popup_layer::blocks_scroll() const noexcept {
        return !m_records.empty();
    }

    void popup_layer::set_bounds(control_id key, rect r) noexcept {
        const auto idx = find_index(key);
        if (idx >= m_records.size()) return;
        m_records[idx].bounds = r;
    }

    void popup_layer::route_input(frame_ctx& ctx) noexcept {
        if (m_records.empty()) return;

        const bool lpressed = ctx.input().key_pressed(VK_LBUTTON);
        const bool rpressed = ctx.input().key_pressed(VK_RBUTTON);
        const bool esc = ctx.input().key_pressed(VK_ESCAPE);

        bool any_claimed = false;

        snapshot_keys();
        for (dh::size_t i = m_pass_keys.size(); i-- > 0;) {
            const auto idx = find_index(m_pass_keys[i]);
            if (idx >= m_records.size()) continue;

            const auto handler = m_records[idx].input;
            if (!handler) continue;
            if (handler(ctx)) {
                any_claimed = true;
                break;
            }
        }

        if (any_claimed) {
            if (lpressed || rpressed) ctx.consume_mouse();

            ctx.lock_wheel();
            return;
        }

        if (lpressed || rpressed || esc) {
            const bool had_open = !m_records.empty();
            close_all();

            if (had_open && (lpressed || rpressed)) {
                ctx.consume_mouse();
            }
        }
    }

    void popup_layer::render(painter& pnt, frame_ctx& ctx) noexcept {
        if (m_records.empty()) return;

        const float vp_w = static_cast<float>(pnt.viewport_w());
        const float vp_h = static_cast<float>(pnt.viewport_h());
        const painter::clip_scope clip{pnt, {0.0f, 0.0f, vp_w, vp_h}};

        const painter::alpha_scope opaque{pnt, 1.0f};

        snapshot_keys();
        for (const auto key : m_pass_keys) {
            const auto idx = find_index(key);
            if (idx >= m_records.size()) continue;
            const auto draw = m_records[idx].draw;
            if (!draw) continue;
            draw(pnt, ctx);
        }
    }

}
