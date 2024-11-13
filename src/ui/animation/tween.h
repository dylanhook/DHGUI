// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "easing.h"
#include "driver.h"
#include "core/dh/memory.h"
#include "core/dh/numeric_limits.h"

namespace dh::ui::animation {

    class tween final {
    public:
        tween() noexcept;
        ~tween();

        tween(const tween&) = delete;
        tween& operator=(const tween&) = delete;
        tween(tween&&) = delete;
        tween& operator=(tween&&) = delete;

        void snap(float v) noexcept;

        void set_target(float v, float duration) noexcept;

        [[nodiscard]] float value() const noexcept { return m_value; }

        [[nodiscard]] float target() const noexcept { return m_target; }

        void advance(float dt) noexcept;

    private:
        friend void detail::register_tween(tween* t) noexcept;
        friend void detail::unregister_tween(tween* t) noexcept;
        friend void driver::tick(float dt) noexcept;

        [[nodiscard]] bool advance_value(float dt) noexcept;
        float m_value{0.0f};
        float m_target{0.0f};
        float m_start{0.0f};
        float m_t{0.0f};
        float m_duration{0.0f};

        static constexpr dh::u32 k_not_registered = dh::numeric_limits<dh::u32>::max();
        dh::u32 m_active_slot{k_not_registered};
    };

}
