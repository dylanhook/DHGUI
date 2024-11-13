// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include <cmath>

namespace dh {

    [[nodiscard]] inline float sqrt(float x) noexcept {
        return ::sqrt(x);
    }

    [[nodiscard]] inline float sin(float x) noexcept {
        return ::sin(x);
    }

    [[nodiscard]] inline float cos(float x) noexcept {
        return ::cos(x);
    }

    [[nodiscard]] inline float tan(float x) noexcept {
        return ::tan(x);
    }

    [[nodiscard]] inline float asin(float x) noexcept {
        return ::asin(x);
    }

    [[nodiscard]] inline float acos(float x) noexcept {
        return ::acos(x);
    }

    [[nodiscard]] inline float atan2(float y, float x) noexcept {
        return ::atan2(y, x);
    }

    [[nodiscard]] inline float pow(float b, float e) noexcept {
        return ::pow(b, e);
    }

    [[nodiscard]] inline float fmod(float x, float y) noexcept {
        return ::fmod(x, y);
    }

    [[nodiscard]] inline float hypot(float x, float y) noexcept {
        return ::hypot(x, y);
    }

    [[nodiscard]] inline float fabs(float x) noexcept {
        return ::fabs(x);
    }

    [[nodiscard]] inline float ceil(float x) noexcept {
        return ::ceil(x);
    }

    [[nodiscard]] inline float floor(float x) noexcept {
        return ::floor(x);
    }

    [[nodiscard]] inline float round(float x) noexcept {
        return ::round(x);
    }

    [[nodiscard]] inline long lround(float x) noexcept {
        return ::lround(x);
    }

    [[nodiscard]] inline bool isfinite(float x) noexcept {
        return ::isfinite(x);
    }

}
