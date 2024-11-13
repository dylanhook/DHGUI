// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "tween.h"

#include "driver.h"

namespace dh::ui::animation {

    tween::tween() noexcept {
        detail::retain_tween_capacity();
    }

    tween::~tween() {
        detail::unregister_tween(this);
        detail::release_tween_capacity();
    }

    void tween::snap(float v) noexcept {
        detail::unregister_tween(this);
        m_value = v;
        m_target = v;
        m_start = v;
        m_t = 0.0f;
        m_duration = 0.0f;
    }

    void tween::set_target(float v, float duration) noexcept {
        if (duration <= 0.0f) {
            snap(v);
            return;
        }
        if (m_target == v) {
            return;
        }

        m_start = m_value;
        m_target = v;
        m_t = 0.0f;
        m_duration = duration;

        detail::register_tween(this);
    }

    void tween::advance(float dt) noexcept {
        if (!advance_value(dt)) detail::unregister_tween(this);
    }

    bool tween::advance_value(float dt) noexcept {
        if (m_t >= m_duration) {
            m_value = m_target;
            return false;
        }
        m_t += dt;
        if (m_t >= m_duration) {
            m_value = m_target;
            return false;
        }
        const float u = m_t / m_duration;
        const float e = easing::cubic_out(u);
        m_value = m_start + (m_target - m_start) * e;
        return true;
    }

}
