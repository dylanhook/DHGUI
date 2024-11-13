// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/core/types.h"

namespace dh::ui::controls {

    struct scroll_metrics final {
    public:
        float window{0.0f};
        float total{0.0f};
        float position{0.0f};
        float track_start{0.0f};
        float track_length{0.0f};
        float thumb_min{0.0f};

        [[nodiscard]] constexpr bool overflows() const noexcept { return total > window; }

        [[nodiscard]] constexpr float span() const noexcept { return dh::max(0.0f, total - window); }

        [[nodiscard]] constexpr float clamped(float pos) const noexcept { return dh::clamp(pos, 0.0f, span()); }

        [[nodiscard]] constexpr float thumb_length() const noexcept {
            if (total <= 0.0f) return track_length;
            return dh::min(track_length, dh::max(thumb_min, track_length * window / total));
        }

        [[nodiscard]] constexpr float thumb_start() const noexcept {
            const float travel = track_length - thumb_length();
            const float sp = span();
            const float frac = sp > 0.0f ? clamped(position) / sp : 0.0f;
            return track_start + travel * frac;
        }

        [[nodiscard]] constexpr bool thumb_holds(float coord) const noexcept {
            const float start = thumb_start();
            return coord >= start && coord < start + thumb_length();
        }

        [[nodiscard]] constexpr float position_for_thumb_start(float start) const noexcept {
            const float travel = track_length - thumb_length();
            if (travel <= 0.0f) return 0.0f;
            const float frac = dh::clamp((start - track_start) / travel, 0.0f, 1.0f);
            return frac * span();
        }

        [[nodiscard]] constexpr int row_position_for_thumb_start(float start) const noexcept {
            return static_cast<int>(position_for_thumb_start(start) + 0.5f);
        }
    };

    [[nodiscard]] constexpr rect popup_scroll_track(rect popup, float pad, float bar_w) noexcept {
        return {popup.right() - 1.0f - bar_w, popup.y + pad, bar_w, popup.h - pad * 2.0f};
    }

    struct popup_scrollbar final {
    public:
        rect popup{};
        float pad{0.0f};
        float bar_w{0.0f};
        float thumb_min{0.0f};
        int visible{0};
        int total{0};
        int first_visible{0};

        [[nodiscard]] constexpr rect track() const noexcept { return popup_scroll_track(popup, pad, bar_w); }

        [[nodiscard]] constexpr scroll_metrics bar() const noexcept {
            const rect t = track();
            return {.window = static_cast<float>(visible),
                .total = static_cast<float>(total),
                .position = static_cast<float>(first_visible),
                .track_start = t.y,
                .track_length = t.h,
                .thumb_min = thumb_min};
        }
    };

}
