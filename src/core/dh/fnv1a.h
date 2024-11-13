// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/memory.h"

namespace dh {

    inline constexpr u64 k_fnv1a64_offset_basis = 0xCBF29CE484222325ULL;
    inline constexpr u64 k_fnv1a64_prime = 0x00000100000001B3ULL;
    inline constexpr u32 k_fnv1a32_offset_basis = 0x811C9DC5u;
    inline constexpr u32 k_fnv1a32_prime = 0x01000193u;

    class fnv1a final {
    public:
        constexpr fnv1a() noexcept = default;
        ~fnv1a() = default;
        constexpr fnv1a(const fnv1a&) noexcept = default;
        constexpr fnv1a& operator=(const fnv1a&) noexcept = default;
        constexpr fnv1a(fnv1a&&) noexcept = default;
        constexpr fnv1a& operator=(fnv1a&&) noexcept = default;

        constexpr void combine(u64 value) noexcept { m_state = (m_state ^ value) * k_fnv1a64_prime; }

        [[nodiscard]] constexpr u64 value() const noexcept { return m_state; }

    private:
        u64 m_state{k_fnv1a64_offset_basis};
    };

}
