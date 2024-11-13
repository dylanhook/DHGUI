// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "stack.h"

#include "ui/theme/theme.h"

namespace dh::ui::layout {

    namespace {

        [[nodiscard]] align resolve_cross(cross_align child, align stack_default) noexcept {
            switch (child) {
                case cross_align::start:
                    return align::start;
                case cross_align::center:
                    return align::center;
                case cross_align::end:
                    return align::end;
                case cross_align::stretch:
                    return align::stretch;
                case cross_align::inherit:
                    return stack_default;
            }
            return stack_default;
        }

        [[nodiscard]] int cross_offset(align a, int child_cross, int avail_cross) noexcept {
            switch (a) {
                case align::start:
                    return 0;
                case align::end:
                    return avail_cross - child_cross;
                case align::center:
                    return (avail_cross - child_cross) / 2;
                case align::stretch:
                    return 0;
            }
            return 0;
        }

        [[nodiscard]] int resolve_spacing(spacing_mode mode, int fixed, const frame_ctx& ctx) noexcept {
            switch (mode) {
                case spacing_mode::fixed:
                    return fixed;
                case spacing_mode::theme_normal:
                    return ctx.theme().metrics().gap_normal;
                case spacing_mode::theme_label_row:
                    return ctx.theme().metrics().label_row_gap;
            }
            return fixed;
        }

    }

    size stack::desired_size(const frame_ctx& ctx) const noexcept {
        int main_total = 0;
        int largest_main = 0;
        int cross_max = 0;
        int visible_count = 0;
        const int spacing = resolve_spacing(m_spacing_mode, m_spacing, ctx);

        for (const auto& child : m_children) {
            if (!child->visible()) continue;
            const auto ds = child->desired_size(ctx);
            const int child_main = (m_orient == orientation::vertical) ? ds.h : ds.w;
            const int child_cross = (m_orient == orientation::vertical) ? ds.w : ds.h;
            const int effective_main = (child->hint().min_main > child_main) ? child->hint().min_main : child_main;
            main_total += effective_main;
            largest_main = (largest_main < effective_main) ? effective_main : largest_main;
            cross_max = (cross_max < child_cross) ? child_cross : cross_max;
            ++visible_count;
        }
        if (m_main_sizing == main_sizing::uniform) main_total = largest_main * visible_count;
        if (visible_count > 1) main_total += spacing * (visible_count - 1);

        main_total += (m_orient == orientation::vertical) ? (m_padding_t + m_padding_b) : (m_padding_l + m_padding_r);
        cross_max += (m_orient == orientation::vertical) ? (m_padding_l + m_padding_r) : (m_padding_t + m_padding_b);

        if (m_orient == orientation::vertical) return {cross_max, main_total};
        return {main_total, cross_max};
    }

    void stack::measure(const frame_ctx& ctx, size available) noexcept {
        m_resolved_main.clear();
        m_resolved_cross.clear();
        m_resolved_main.reserve(m_children.size());
        m_resolved_cross.reserve(m_children.size());
        m_resolved_spacing = resolve_spacing(m_spacing_mode, m_spacing, ctx);

        const int avail_main = (m_orient == orientation::vertical) ? (available.h - m_padding_t - m_padding_b) : (available.w - m_padding_l - m_padding_r);
        const int avail_cross = (m_orient == orientation::vertical) ? (available.w - m_padding_l - m_padding_r) : (available.h - m_padding_t - m_padding_b);

        int main_used = 0;
        int largest_main = 0;
        int visible_count = 0;
        float total_flex = 0.0f;

        for (const auto& child : m_children) {
            if (!child->visible()) {
                m_resolved_main.push_back(0);
                m_resolved_cross.push_back(0);
                continue;
            }
            const size ds = child->desired_size(ctx);
            const int baseline_main = (m_orient == orientation::vertical) ? ds.h : ds.w;
            const int baseline_cross = (m_orient == orientation::vertical) ? ds.w : ds.h;
            const int effective_main = (child->hint().min_main > baseline_main) ? child->hint().min_main : baseline_main;

            const auto resolved_cross = (resolve_cross(child->hint().cross, m_cross_align) == align::stretch) ? avail_cross : baseline_cross;

            m_resolved_main.push_back(effective_main);
            m_resolved_cross.push_back(resolved_cross);

            main_used += effective_main;
            largest_main = (largest_main < effective_main) ? effective_main : largest_main;
            total_flex += child->hint().flex;
            ++visible_count;
        }

        const int spacing_total = (visible_count > 1) ? m_resolved_spacing * (visible_count - 1) : 0;
        main_used += spacing_total;

        if (m_main_sizing == main_sizing::uniform && visible_count > 0) {
            const int available_each = (avail_main - spacing_total) / visible_count;
            // uniform sizing keeps the largest requested size even if the parent is smaller.
            const int uniform_main = (largest_main < available_each) ? available_each : largest_main;
            for (dh::size_t i = 0; i < m_children.size(); ++i) {
                if (m_children[i]->visible()) m_resolved_main[i] = uniform_main;
            }
        } else {
            const int residual = avail_main - main_used;
            if (residual > 0 && total_flex > 0.0f) {
                // divide the remaining space each time so rounding doesnt lose pixels.
                float remaining = static_cast<float>(residual);
                float remaining_flex = total_flex;
                for (dh::size_t i = 0; i < m_children.size(); ++i) {
                    const auto& ch = m_children[i];
                    if (!ch->visible()) continue;
                    const float f = ch->hint().flex;
                    if (f <= 0.0f) continue;
                    int give = 0;
                    if (remaining_flex > 0.0f) {
                        give = static_cast<int>((remaining * f) / remaining_flex + 0.5f);
                    }
                    if (give > static_cast<int>(remaining)) give = static_cast<int>(remaining);
                    m_resolved_main[i] += give;
                    remaining -= static_cast<float>(give);
                    remaining_flex -= f;
                }
            }
        }

        for (dh::size_t i = 0; i < m_children.size(); ++i) {
            const auto& ch = m_children[i];
            if (!ch->visible()) continue;
            const int main_i = m_resolved_main[i];
            const int cross_i = m_resolved_cross[i];
            const size box = (m_orient == orientation::vertical) ? size{cross_i, main_i} : size{main_i, cross_i};
            ch->measure(ctx, box);
        }
    }

    void stack::arrange(rect r) noexcept {
        m_rect = r;

        const int avail_main =
            (m_orient == orientation::vertical) ? static_cast<int>(r.h) - m_padding_t - m_padding_b : static_cast<int>(r.w) - m_padding_l - m_padding_r;
        const int avail_cross =
            (m_orient == orientation::vertical) ? static_cast<int>(r.w) - m_padding_l - m_padding_r : static_cast<int>(r.h) - m_padding_t - m_padding_b;

        int main_used = 0;
        int visible_count = 0;
        for (dh::size_t i = 0; i < m_children.size(); ++i) {
            if (!m_children[i]->visible()) continue;
            main_used += m_resolved_main[i];
            ++visible_count;
        }
        if (visible_count > 1) main_used += m_resolved_spacing * (visible_count - 1);

        int main_origin = 0;
        switch (m_main_align) {
            case align::start:
                main_origin = 0;
                break;
            case align::end:
                main_origin = avail_main - main_used;
                break;
            case align::center:
                main_origin = (avail_main - main_used) / 2;
                break;
            case align::stretch:
                main_origin = 0;
                break;
        }
        if (main_origin < 0) main_origin = 0;

        int cursor = main_origin;
        for (dh::size_t i = 0; i < m_children.size(); ++i) {
            const auto& ch = m_children[i];
            if (!ch->visible()) continue;
            const int main_i = m_resolved_main[i];
            const int cross_i = m_resolved_cross[i];
            const align cross_a = resolve_cross(ch->hint().cross, m_cross_align);
            const int cross_off = cross_offset(cross_a, cross_i, avail_cross);

            rect child_r;
            if (m_orient == orientation::vertical) {
                child_r = {
                    r.x + static_cast<float>(m_padding_l + cross_off),
                    r.y + static_cast<float>(m_padding_t + cursor),
                    static_cast<float>(cross_i),
                    static_cast<float>(main_i),
                };
            } else {
                child_r = {
                    r.x + static_cast<float>(m_padding_l + cursor),
                    r.y + static_cast<float>(m_padding_t + cross_off),
                    static_cast<float>(main_i),
                    static_cast<float>(cross_i),
                };
            }
            ch->arrange(child_r);
            cursor += main_i + m_resolved_spacing;
        }
    }

    void stack::paint(frame_ctx& ctx) noexcept {
        for (const auto& ch : m_children) {
            if (!ch->visible()) continue;
            ch->paint(ctx);
        }
    }

    bool stack::on_input(frame_ctx& ctx) noexcept {
        bool any = false;

        // later children are drawn on top, let them take input first.
        for (dh::size_t i = m_children.size(); i > 0; --i) {
            const auto& ch = m_children[i - 1];
            if (!ch->visible()) continue;
            any |= ch->on_input(ctx);
        }
        return any;
    }

}
