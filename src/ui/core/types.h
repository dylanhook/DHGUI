// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/algorithm.h"
#include "core/dh/memory.h"

namespace dh::ui {

    struct point final {
        float x{0.0f};
        float y{0.0f};

        constexpr point() noexcept = default;
        constexpr point(const point&) noexcept = default;
        constexpr point(point&&) noexcept = default;
        constexpr point& operator=(const point&) noexcept = default;
        constexpr point& operator=(point&&) noexcept = default;
        ~point() = default;

        constexpr point(float in_x, float in_y) noexcept : x(in_x), y(in_y) {}

        [[nodiscard]] constexpr point operator+(const point& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }

        [[nodiscard]] constexpr point operator-(const point& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }

        [[nodiscard]] constexpr bool operator==(const point&) const noexcept = default;
    };

    struct size final {
        int w{0};
        int h{0};

        constexpr size() noexcept = default;
        constexpr size(const size&) noexcept = default;
        constexpr size(size&&) noexcept = default;
        constexpr size& operator=(const size&) noexcept = default;
        constexpr size& operator=(size&&) noexcept = default;
        ~size() = default;

        constexpr size(int in_w, int in_h) noexcept : w(in_w), h(in_h) {}

        [[nodiscard]] constexpr bool operator==(const size&) const noexcept = default;
    };

    struct rect final {
        float x{0.0f};
        float y{0.0f};
        float w{0.0f};
        float h{0.0f};

        constexpr rect() noexcept = default;
        constexpr rect(const rect&) noexcept = default;
        constexpr rect(rect&&) noexcept = default;
        constexpr rect& operator=(const rect&) noexcept = default;
        constexpr rect& operator=(rect&&) noexcept = default;
        ~rect() = default;

        constexpr rect(float in_x, float in_y, float in_w, float in_h) noexcept : x(in_x), y(in_y), w(in_w), h(in_h) {}

        [[nodiscard]] constexpr float right() const noexcept { return x + w; }

        [[nodiscard]] constexpr float bottom() const noexcept { return y + h; }

        [[nodiscard]] constexpr point center() const noexcept { return {x + w * 0.5f, y + h * 0.5f}; }

        [[nodiscard]] constexpr bool contains(point p) const noexcept { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }

        [[nodiscard]] constexpr rect inset(float by) const noexcept { return {x + by, y + by, w - by * 2.0f, h - by * 2.0f}; }

        [[nodiscard]] constexpr rect inset(float lr, float tb) const noexcept { return {x + lr, y + tb, w - lr * 2.0f, h - tb * 2.0f}; }

        [[nodiscard]] constexpr bool operator==(const rect&) const noexcept = default;
    };

    struct control_id final {
        dh::u64 value{0};

        constexpr control_id() noexcept = default;
        constexpr control_id(const control_id&) noexcept = default;
        constexpr control_id(control_id&&) noexcept = default;
        constexpr control_id& operator=(const control_id&) noexcept = default;
        constexpr control_id& operator=(control_id&&) noexcept = default;
        ~control_id() = default;

        explicit constexpr control_id(dh::u64 v) noexcept : value(v) {}

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }

        [[nodiscard]] constexpr bool operator==(const control_id&) const noexcept = default;
    };

    inline constexpr dh::u64 k_control_id_salt = 0xC0DEC0DEull;

    [[nodiscard]] inline control_id make_control_id(const void* p) noexcept {
        return control_id{reinterpret_cast<dh::u64>(p) ^ k_control_id_salt};
    }

}
