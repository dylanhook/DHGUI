// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh::math {

    struct vector2 final {
        float x{0.0f};
        float y{0.0f};

        constexpr vector2() noexcept = default;
        constexpr vector2(const vector2&) noexcept = default;
        constexpr vector2(vector2&&) noexcept = default;
        constexpr vector2& operator=(const vector2&) noexcept = default;
        constexpr vector2& operator=(vector2&&) noexcept = default;
        ~vector2() = default;

        constexpr vector2(float in_x, float in_y) noexcept : x(in_x), y(in_y) {}

        [[nodiscard]] constexpr vector2 operator+(const vector2& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }

        [[nodiscard]] constexpr vector2 operator-(const vector2& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }

        [[nodiscard]] constexpr vector2 operator*(float s) const noexcept { return {x * s, y * s}; }

        [[nodiscard]] constexpr bool operator==(const vector2&) const noexcept = default;
    };

    static_assert(sizeof(vector2) == 8, "vector2 must be 8 bytes without padding");

}
