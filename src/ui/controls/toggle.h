// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/array.h"
#include "core/dh/function.h"
#include "core/dh/optional.h"
#include "core/dh/span.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "math/color.h"
#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/core/context_editor.h"
#include "ui/core/control.h"

#include "core/dh/memory.h"

namespace dh::ui::controls {

    class color_picker;
    class slot_strip;
    class list_link;
    class slider;

    void paint_toggle_box(frame_ctx& ctx, rect bounds, float checked, float hover) noexcept;

    class toggle final : public control {
    public:
        toggle(dh::string label, reactive::signal<bool>& bound, const i_context_editor_factory* editors) noexcept;
        ~toggle() override;

        toggle(const toggle&) = delete;
        toggle& operator=(const toggle&) = delete;
        toggle(toggle&&) = delete;
        toggle& operator=(toggle&&) = delete;

        toggle* with_context_editor(dh::unique_ptr<i_context_editor> editor) noexcept;

        toggle* with_color(reactive::signal<math::color>& bound) noexcept;

        toggle* with_colors(reactive::signal<math::color>& first, reactive::signal<math::color>& second) noexcept;

        toggle* with_colors(dh::span<reactive::signal<math::color>* const> bound, dh::function<dh::size_t()> visible = {}) noexcept;

        toggle* with_slots(reactive::signal<int>& selected, dh::size_t count, int first_value, dh::function<dh::optional<math::color>(dh::size_t)> fill) noexcept;

        toggle* with_list_link(dh::function<void()> open, dh::function<bool()> visible) noexcept;

        toggle* with_slider_value(slider& value_slider) noexcept;

        toggle* set_editor_name(dh::string display_name) noexcept;

        toggle* with_status(dh::function<dh::string_view()> status) noexcept;

        [[nodiscard]] color_picker* color_at(dh::size_t index) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] bool list_link_visible() const noexcept;
        [[nodiscard]] int trailing_reserved_w(const frame_ctx& ctx) const noexcept;
        [[nodiscard]] rect primary_rect(const frame_ctx& ctx) const noexcept;
        void arrange_trailing(const frame_ctx& ctx) noexcept;

        [[nodiscard]] float value_right_edge(const frame_ctx& ctx) const noexcept;
        [[nodiscard]] rect slider_value_bounds(const frame_ctx& ctx) const noexcept;

        dh::string m_label;
        reactive::signal<bool>* m_bound;
        animation::tween m_check_t;
        animation::tween m_hover_t;
        dh::u64 m_last_paint_seq{0};
        bool m_painted{false};

        static constexpr dh::size_t k_max_inline_colors = 8;

        [[nodiscard]] dh::size_t visible_colors() const noexcept;

        dh::array<dh::unique_ptr<color_picker>, k_max_inline_colors> m_colors{};
        dh::size_t m_color_count{0};
        dh::function<dh::size_t()> m_visible_colors;
        dh::unique_ptr<i_context_editor> m_context_editor;
        dh::unique_ptr<slot_strip> m_slots;
        dh::unique_ptr<list_link> m_list_link;
        dh::function<bool()> m_list_link_visible;
        slider* m_value_slider{nullptr};
        dh::function<dh::string_view()> m_status;
    };

}
