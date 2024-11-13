// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "label.h"

#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

#include "core/dh/string.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        template <typename Fn> void for_each_wrapped_line(const painter& pnt, dh::string_view s, float max_w, Fn&& emit) noexcept {
            dh::size_t start = 0;
            while (start < s.size()) {
                if (pnt.measure_text_w(s.substr(start)) <= max_w) {
                    emit(s.substr(start));
                    return;
                }

                dh::size_t best = dh::string_view::npos;
                for (dh::size_t j = start; j < s.size(); ++j) {
                    if (s[j] != ' ') {
                        continue;
                    }
                    if (pnt.measure_text_w(s.substr(start, j - start)) <= max_w) {
                        best = j;
                    } else {
                        break;
                    }
                }
                if (best == dh::string_view::npos) {
                    dh::size_t space = dh::string_view::npos;
                    for (dh::size_t j = start; j < s.size(); ++j) {
                        if (s[j] == ' ') {
                            space = j;
                            break;
                        }
                    }
                    if (space == dh::string_view::npos) {
                        emit(s.substr(start));
                        return;
                    }
                    emit(s.substr(start, space - start));
                    start = space + 1;
                } else {
                    emit(s.substr(start, best - start));
                    start = best + 1;
                }
            }
        }

    }

    label::label(dh::string text) noexcept : m_text(dh::move(text)) {}

    void label::set_text(dh::string text) noexcept {
        m_text = dh::move(text);
    }

    void label::set_color(math::color c) noexcept {
        m_override_color = c;
    }

    void label::set_wrap(bool on) noexcept {
        m_wrap = on;
    }

    size label::desired_size(const frame_ctx& ctx) const noexcept {
        const dh::string_view sv{m_text.data(), m_text.size()};
        const int line_h = ctx.pnt().text_line_h();

        // let the parent pick the width, or wrapping keeps feeding back the old width.
        if (m_wrap) {
            int lines = 1;
            if (m_rect.w > 1.0f) {
                lines = 0;
                for_each_wrapped_line(ctx.pnt(), sv, m_rect.w, [&lines](dh::string_view) noexcept { ++lines; });
                if (lines < 1) lines = 1;
            }
            return {0, line_h * lines};
        }

        const float w = ctx.pnt().measure_text_w(sv);
        return {static_cast<int>(dh::ceil(w)), line_h};
    }

    void label::paint(frame_ctx& ctx) noexcept {
        if (m_text.empty()) return;
        const math::color c = m_override_color.value_or(ctx.theme().colors().text_label);
        const int line_h = ctx.pnt().text_line_h();
        const dh::string_view sv{m_text.data(), m_text.size()};

        if (m_wrap && m_rect.w > 1.0f) {
            float ty = m_rect.y - painter::k_text_center_bias;
            for_each_wrapped_line(ctx.pnt(), sv, m_rect.w, [&](dh::string_view line) noexcept {
                ctx.pnt().text({m_rect.x, ty}, line, c);
                ty += static_cast<float>(line_h);
            });
            return;
        }

        ctx.pnt().text_truncated({m_rect.x, ctx.pnt().text_center_y(m_rect)}, sv, m_rect.w, c);
    }

}
