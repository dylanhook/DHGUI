// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/function.h"
#include "ui/animation/tween.h"
#include "ui/core/control.h"

#include "core/dh/memory.h"

namespace dh::ui::controls {

    class list_link final : public control {
    public:
        explicit list_link(dh::function<void()> open) noexcept;
        ~list_link() override = default;

        list_link(const list_link&) = delete;
        list_link& operator=(const list_link&) = delete;
        list_link(list_link&&) = delete;
        list_link& operator=(list_link&&) = delete;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] rect square_rect(const frame_ctx& ctx) const noexcept;

        dh::function<void()> m_open;
        animation::tween m_hover;
        dh::u64 m_last_paint_seq{0};
        bool m_painted{false};
    };

}
