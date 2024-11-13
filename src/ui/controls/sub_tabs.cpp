// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "sub_tabs.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        constexpr float k_chip_gap = 4.0f;
        constexpr int k_body_gap = 10;
    }

    sub_tabs::sub_tabs() noexcept = default;

    void sub_tabs::make_scrollable(int viewport_h) noexcept {
        m_scrollable = true;
        m_scroll_h = viewport_h;
    }

    layout::stack& sub_tabs::add_page(dh::string name) noexcept {
        auto t = dh::make_unique<animation::tween>();
        if (!t) dh::alloc_failed("ui::controls::sub_tabs page tween");
        t->snap(0.0f);

        if (m_scrollable) {
            auto host = adopt_child(dh::make_unique<scroll_view>(m_scroll_h));
            if (!host) dh::alloc_failed("ui::controls::sub_tabs scroll page");
            host->content().set_spacing(4);
            host->content().set_cross_align(layout::align::stretch);
            layout::stack* body = &host->content();
            host->set_visible(m_pages.empty() ? visibility::shown : visibility::hidden);
            m_pages.push_back({dh::move(name), dh::move(host), body, dh::move(t)});
            return *body;
        }

        auto host = adopt_child(dh::make_unique<layout::stack>(layout::orientation::vertical));
        if (!host) dh::alloc_failed("ui::controls::sub_tabs page");
        host->set_spacing(4);
        host->set_cross_align(layout::align::stretch);
        layout::stack* body = host.get();
        host->set_visible(m_pages.empty() ? visibility::shown : visibility::hidden);
        m_pages.push_back({dh::move(name), dh::move(host), body, dh::move(t)});
        return *body;
    }

    void sub_tabs::set_active(int index) noexcept {
        if (index < 0 || index >= static_cast<int>(m_pages.size())) return;
        if (index == m_active) return;
        m_pages[static_cast<dh::size_t>(m_active)].host->set_visible(visibility::hidden);
        m_active = index;
        m_pages[static_cast<dh::size_t>(m_active)].host->set_visible(visibility::shown);
    }

    size sub_tabs::desired_size(const frame_ctx& ctx) const noexcept {
        if (m_pages.empty()) return {0, ctx.theme().metrics().subtab_h};
        size body_size{0, 0};
        for (const auto& p : m_pages) {
            const auto bs = p.host->desired_size(ctx);
            body_size.w = dh::max(body_size.w, bs.w);
            body_size.h = dh::max(body_size.h, bs.h);
        }

        return {body_size.w, ctx.theme().metrics().subtab_h + k_body_gap + body_size.h};
    }

    void sub_tabs::measure(const frame_ctx& ctx, size available) noexcept {
        m_subtab_h = ctx.theme().metrics().subtab_h;
        if (m_pages.empty()) return;

        const size body_box{available.w, available.h - m_subtab_h - k_body_gap};
        for (auto& p : m_pages) {
            p.host->measure(ctx, body_box);
        }
    }

    rect sub_tabs::strip_rect() const noexcept {
        return {m_rect.x, m_rect.y, m_rect.w, static_cast<float>(m_subtab_h)};
    }

    rect sub_tabs::chip_rect(dh::size_t index) const noexcept {
        if (m_pages.empty()) return {};
        const rect strip = strip_rect();
        const float total_gap = k_chip_gap * static_cast<float>(m_pages.size() - 1);
        const float seg_w = (strip.w - total_gap) / static_cast<float>(m_pages.size());
        return {strip.x + (seg_w + k_chip_gap) * static_cast<float>(index), strip.y, seg_w, strip.h};
    }

    rect sub_tabs::body_rect() const noexcept {
        const float body_y = m_rect.y + static_cast<float>(m_subtab_h + k_body_gap);
        return {m_rect.x, body_y, m_rect.w, m_rect.bottom() - body_y};
    }

    void sub_tabs::arrange(rect r) noexcept {
        m_rect = r;
        if (m_pages.empty()) return;

        m_pages[static_cast<dh::size_t>(m_active)].host->arrange(body_rect());
    }

    void sub_tabs::paint(frame_ctx& ctx) noexcept {
        if (m_pages.empty()) return;

        const auto& pal = ctx.theme().colors();

        for (dh::size_t i = 0; i < m_pages.size(); ++i) {
            const auto& p = m_pages[i];
            const rect seg = chip_rect(i);
            const bool active = (static_cast<int>(i) == m_active);
            const float h = p.hover_t->value();

            ctx.pnt().pill_button(seg, pal, active ? chip_state::active : chip_state::rest, h);

            math::color tc;
            if (active)
                tc = ctx.theme().accent_value();
            else if (h > 0.0f)
                tc = pal.text_secondary;
            else
                tc = pal.text_muted;
            ctx.pnt().text_centered(seg, dh::string_view{p.name.data(), p.name.size()}, tc);
        }

        m_pages[static_cast<dh::size_t>(m_active)].host->paint(ctx);
    }

    bool sub_tabs::on_input(frame_ctx& ctx) noexcept {
        if (m_pages.empty()) return false;

        const point cursor = ctx.cursor();

        for (dh::size_t i = 0; i < m_pages.size(); ++i) {
            const bool inside = chip_rect(i).contains(cursor);
            m_pages[i].hover_t->set_target(inside ? 1.0f : 0.0f, ctx.theme().times().hover);
            if (inside && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                if (static_cast<int>(i) != m_active) {
                    set_active(static_cast<int>(i));
                }
                ctx.consume_mouse();
                return true;
            }
        }

        auto& active_host = *m_pages[static_cast<dh::size_t>(m_active)].host;
        return active_host.on_input(ctx);
    }

}
