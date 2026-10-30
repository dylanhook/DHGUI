// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "groupbox.h"

#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include "core/dh/memory.h"
#include "core/dh/string.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        constexpr int k_scroll_edge_inset = 3;

    }

    groupbox::groupbox(dh::string title) noexcept : m_title(dh::move(title)), m_inner(adopt_child(dh::make_unique<layout::stack>(layout::orientation::vertical))) {
        if (!m_inner) dh::alloc_failed("ui::controls::groupbox");

        m_inner->set_spacing(4);
        m_inner->set_cross_align(layout::align::stretch);
    }

    void groupbox::make_scrollable(int max_h) noexcept {
        if (m_scroll) return;

        m_scroll = adopt_child(dh::make_unique<scroll_view>(max_h));
        if (!m_scroll) dh::alloc_failed("ui::controls::groupbox scroll");
        m_scroll->content().set_spacing(4);
        m_scroll->content().set_cross_align(layout::align::stretch);

        m_inner.reset();
    }

    void groupbox::make_appended_content_scrollable() noexcept {
        if (m_scroll) return;

        auto scroll = dh::make_unique<scroll_view>(0);
        if (!scroll) dh::alloc_failed("ui::controls::groupbox appended scroll");
        scroll->content().set_spacing(4);
        scroll->content().set_cross_align(layout::align::stretch);

        m_desired_content = m_inner.get();
        scroll->content().add(dh::move(m_inner));
        m_scroll = adopt_child(dh::move(scroll));
    }

    layout::stack& groupbox::inner() noexcept {
        return m_scroll ? m_scroll->content() : *m_inner;
    }

    size groupbox::desired_size(const frame_ctx& ctx) const noexcept {
        const auto& m = ctx.theme().metrics();

        if (m_fixed_h >= 0) {
            return {0, m_fixed_h};
        }

        const size c = m_desired_content != nullptr ? m_desired_content->desired_size(ctx) : (m_scroll ? m_scroll->desired_size(ctx) : m_inner->desired_size(ctx));
        return {c.w + m.group_padding_x * 2, c.h + m.group_padding_top + m.group_padding_bottom};
    }

    void groupbox::measure(const frame_ctx& ctx, size available) noexcept {
        const auto& m = ctx.theme().metrics();
        m_padding_x = m.group_padding_x;
        m_padding_top = m.group_padding_top;
        m_padding_bottom = m.group_padding_bottom;

        const int content_h = available.h - m_padding_top - m_padding_bottom;
        if (m_scroll) {
            m_scroll->measure(ctx, {available.w - m_padding_x - k_scroll_edge_inset, content_h});
            return;
        }
        m_inner->measure(ctx, {available.w - m_padding_x * 2, content_h});
    }

    void groupbox::arrange(rect r) noexcept {
        m_rect = r;

        const float body_x = r.x + static_cast<float>(m_padding_x);
        const float body_y = r.y + static_cast<float>(m_padding_top);
        const float body_h = r.h - static_cast<float>(m_padding_top + m_padding_bottom);
        if (m_scroll) {
            m_scroll->arrange({body_x, body_y, r.w - static_cast<float>(m_padding_x + k_scroll_edge_inset), body_h});
        } else {
            m_inner->arrange({body_x, body_y, r.w - static_cast<float>(m_padding_x * 2), body_h});
        }
    }

    void groupbox::paint(frame_ctx& ctx) noexcept {
        auto& pnt = ctx.pnt();
        const auto& pal = ctx.theme().colors();
        const auto& metrics = ctx.theme().metrics();
        const float line = static_cast<float>(pnt.text_line_h());
        const rect border{m_rect.x, m_rect.y + line * 0.5f, m_rect.w, m_rect.h - line * 0.5f};
        const dh::string_view title{m_title.data(), m_title.size()};
        const float x = m_rect.x + static_cast<float>(metrics.group_title_inset);
        const float pad = static_cast<float>(metrics.gap_normal);
        const float title_left = title.empty() ? border.x : x - pad;
        const float title_right = title.empty() ? border.x : x + pnt.measure_text_w(title) + pad;
        pnt.group_frame(border, title_left, title_right, pal);
        pnt.text({x, m_rect.y}, title, pal.text_label);
        if (m_scroll) m_scroll->paint(ctx);
        else m_inner->paint(ctx);
    }

    bool groupbox::on_input(frame_ctx& ctx) noexcept {
        return m_scroll ? m_scroll->on_input(ctx) : m_inner->on_input(ctx);
    }

}
