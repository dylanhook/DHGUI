// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/function.h"
#include "core/dh/string.h"
#include "math/color.h"
#include "reactive/signal.h"
#include "ui/core/control.h"
#include "ui/popup/handle.h"
#include "core/dh/memory.h"

namespace dh::ui::controls {

    class color_picker final : public control {
    public:
        color_picker(dh::string label, reactive::signal<math::color>& bound) noexcept;

        ~color_picker() override = default;

        color_picker(const color_picker&) = delete;
        color_picker& operator=(const color_picker&) = delete;
        color_picker(color_picker&&) = delete;
        color_picker& operator=(color_picker&&) = delete;

        void set_alpha_enabled(bool enabled) noexcept;

        void set_reset_provider(dh::function<math::color()> provider) noexcept;

        void set_commit_on_release(bool enable) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] math::color displayed_color() const noexcept;

        enum class drag : dh::u8 { none, sv, hue, alpha };

        void open_picker(frame_ctx& ctx) noexcept;
        void close_picker() noexcept;
        void open_context(frame_ctx& ctx) noexcept;

        void paint_picker(painter& pnt, frame_ctx& ctx) noexcept;
        bool input_picker(frame_ctx& ctx) noexcept;

        void paint_context(painter& pnt, frame_ctx& ctx) noexcept;
        bool input_context(frame_ctx& ctx) noexcept;

        [[nodiscard]] rect compute_picker_bounds(const frame_ctx& ctx) const noexcept;
        [[nodiscard]] rect compute_swatch_rect_ctx(const frame_ctx& ctx) const noexcept;

        struct strips final {
            rect saturation_value{};
            rect hue{};
            rect alpha{};
        };

        [[nodiscard]] strips picker_strips(rect popup) const noexcept;

        void sync_hsv_from_color() noexcept;
        void publish_color() noexcept;

        dh::string m_label;
        reactive::signal<math::color>* m_bound;

        float m_hue{0.0f};
        float m_sat{1.0f};
        float m_val{1.0f};
        float m_alpha{1.0f};
        bool m_alpha_enabled{true};
        math::color m_last_synced{};

        drag m_drag{drag::none};
        int m_context_hover_idx{-1};

        math::color m_initial_color{};
        dh::function<math::color()> m_reset_provider;
        bool m_commit_on_release{false};

        popup_handle m_picker{*this, popup_slot::primary};
        popup_handle m_context{*this, popup_slot::picker_menu};
    };

}
