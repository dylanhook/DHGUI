// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "driver.h"

#include "tween.h"
#include "core/dh/vector.h"

namespace dh::ui::animation {

    namespace {

        constinit dh::vector<tween*> g_tweens;
        constinit dh::size_t g_live_tweens{0};

    }

    namespace detail {

        void retain_tween_capacity() noexcept {
            constexpr dh::size_t k_max_live_tweens = dh::numeric_limits<dh::u32>::max();
            if (g_live_tweens >= k_max_live_tweens) dh::fail_fast("too many tweens", FAST_FAIL_INVALID_ARG);
            // reserve when tweens are created so starting an animation doesnt allocate.
            g_tweens.reserve(g_live_tweens + 1);
            ++g_live_tweens;
        }

        void release_tween_capacity() noexcept {
            --g_live_tweens;
        }

        void register_tween(tween* t) noexcept {
            if (t == nullptr || t->m_active_slot != tween::k_not_registered) return;
            t->m_active_slot = static_cast<dh::u32>(g_tweens.size());
            g_tweens.push_back(t);
        }

        void unregister_tween(tween* t) noexcept {
            if (t == nullptr || t->m_active_slot == tween::k_not_registered) return;
            tween* const moved = g_tweens.back();
            g_tweens[t->m_active_slot] = moved;
            // repair its slot now, the moved tween can unregister before the next tick.
            moved->m_active_slot = t->m_active_slot;
            g_tweens.pop_back();
            t->m_active_slot = tween::k_not_registered;
        }

    }

    namespace driver {

        void tick(float dt) noexcept {
            if (dt <= 0.0f) return;
            dh::size_t index = 0;
            while (index < g_tweens.size()) {
                tween* const current = g_tweens[index];
                if (current->advance_value(dt)) ++index;
                else detail::unregister_tween(current);
            }
        }

    }

}
