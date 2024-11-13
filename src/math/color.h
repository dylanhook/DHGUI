// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"

namespace dh::math {

    struct color final {
        dh::u8 r{0};
        dh::u8 g{0};
        dh::u8 b{0};
        dh::u8 a{255};

        constexpr color() noexcept = default;
        constexpr color(const color&) noexcept = default;
        constexpr color(color&&) noexcept = default;
        constexpr color& operator=(const color&) noexcept = default;
        constexpr color& operator=(color&&) noexcept = default;
        ~color() = default;

        constexpr color(dh::u8 r_in, dh::u8 g_in, dh::u8 b_in, dh::u8 a_in = 255) noexcept : r(r_in), g(g_in), b(b_in), a(a_in) {}

        [[nodiscard]] constexpr float r_norm() const noexcept { return static_cast<float>(r) / 255.0f; }

        [[nodiscard]] constexpr float g_norm() const noexcept { return static_cast<float>(g) / 255.0f; }

        [[nodiscard]] constexpr float b_norm() const noexcept { return static_cast<float>(b) / 255.0f; }

        [[nodiscard]] constexpr float a_norm() const noexcept { return static_cast<float>(a) / 255.0f; }

        [[nodiscard]] static constexpr color white() noexcept { return {255, 255, 255}; }

        [[nodiscard]] static constexpr color black() noexcept { return {0, 0, 0}; }

        [[nodiscard]] static constexpr color red() noexcept { return {255, 0, 0}; }

        [[nodiscard]] static constexpr color green() noexcept { return {0, 255, 0}; }

        [[nodiscard]] static constexpr color blue() noexcept { return {0, 0, 255}; }

        [[nodiscard]] constexpr bool operator==(const color&) const noexcept = default;
    };

    using color_channel = dh::u8;

    [[nodiscard]] constexpr color_channel lerp(color_channel a, color_channel b, float t) noexcept {
        if (t <= 0.0f) return a;
        if (t >= 1.0f) return b;
        const float v = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
        return static_cast<color_channel>(v + 0.5f);
    }

    [[nodiscard]] constexpr color scaled_alpha(const color& c, float scale) noexcept {
        color out = c;
        out.a = static_cast<color_channel>(static_cast<float>(c.a) * dh::clamp(scale, 0.0f, 1.0f) + 0.5f);
        return out;
    }

    [[nodiscard]] constexpr color lerp(const color& a, const color& b, float t) noexcept {
        return {
            lerp(a.r, b.r, t),
            lerp(a.g, b.g, t),
            lerp(a.b, b.b, t),
            lerp(a.a, b.a, t),
        };
    }

    [[nodiscard]] inline float srgb_to_linear(float encoded) noexcept {
        constexpr float k_cutoff = 0.04045f;
        constexpr float k_linear_slope = 12.92f;
        constexpr float k_offset = 0.055f;
        constexpr float k_scale = 1.055f;
        constexpr float k_exponent = 2.4f;
        const float value = dh::clamp(encoded, 0.0f, 1.0f);
        if (value <= k_cutoff) return value / k_linear_slope;
        return dh::pow((value + k_offset) / k_scale, k_exponent);
    }

    [[nodiscard]] inline float linear_to_srgb(float linear) noexcept {
        constexpr float k_cutoff = 0.0031308f;
        constexpr float k_linear_slope = 12.92f;
        constexpr float k_offset = 0.055f;
        constexpr float k_scale = 1.055f;
        constexpr float k_exponent = 1.0f / 2.4f;
        const float value = dh::clamp(linear, 0.0f, 1.0f);
        if (value <= k_cutoff) return value * k_linear_slope;
        return k_scale * dh::pow(value, k_exponent) - k_offset;
    }

}
