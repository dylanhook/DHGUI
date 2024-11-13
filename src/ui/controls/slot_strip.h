// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/array.h"
#include "core/dh/function.h"
#include "core/dh/optional.h"
#include "math/color.h"
#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/core/control.h"

#include "core/dh/memory.h"

namespace dh::ui::controls {

    struct slot_square final {
        rect inside{};
        math::color ink{};
    };

    [[nodiscard]] slot_square paint_slot_square(frame_ctx& ctx, rect square, bool on, float hover) noexcept;

    class slot_strip final : public control {
    public:
        slot_strip(reactive::signal<int>& selected, dh::size_t count, int first_value, dh::function<dh::optional<math::color>(dh::size_t)> fill) noexcept;
        ~slot_strip() override = default;

        slot_strip(const slot_strip&) = delete;
        slot_strip& operator=(const slot_strip&) = delete;
        slot_strip(slot_strip&&) = delete;
        slot_strip& operator=(slot_strip&&) = delete;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

        static constexpr dh::size_t k_max_slots = 6;

    private:
        [[nodiscard]] rect square_rect(const frame_ctx& ctx, dh::size_t index) const noexcept;

        reactive::signal<int>* m_selected;
        dh::size_t m_count;
        int m_first_value;
        dh::function<dh::optional<math::color>(dh::size_t)> m_fill;

        dh::array<animation::tween, k_max_slots> m_hover{};
        dh::u64 m_last_paint_seq{0};
        bool m_painted{false};
    };

}
