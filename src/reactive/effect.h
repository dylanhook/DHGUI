// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "runtime.h"

#include "core/dh/function.h"

#include "core/dh/utility.h"

namespace dh::reactive {

    class effect final : public detail::observer {
    public:
        // runs during construction, anything captured by the callback must already be ready.
        explicit effect(dh::function<void()> body) noexcept : m_body(dh::move(body)) { run(); }

        ~effect() override = default;

        effect(const effect&) = delete;
        effect& operator=(const effect&) = delete;
        effect(effect&&) = delete;
        effect& operator=(effect&&) = delete;

        void notify_dirty() noexcept override {
            if (m_running) return;
            detail::schedule_effect(this);
        }

        void run_scheduled() noexcept override { run(); }

    private:
        void run() noexcept {
            // conditional reads can change, rebuild the subscriptions on every run.
            disconnect_dependencies();
            m_running = true;

            {
                detail::observer_scope scope{this};
                if (m_body) m_body();
            }

            m_running = false;
        }

        dh::function<void()> m_body;
        bool m_running{false};
    };

}
