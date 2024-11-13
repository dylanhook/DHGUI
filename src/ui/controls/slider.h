// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/core/context_editor.h"
#include "ui/core/control.h"

#include "core/dh/function.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"

namespace dh::ui::controls {

    class slider final : public control {
    public:
        slider(dh::string label, reactive::signal<float>& bound, float min, float max, float step, const i_context_editor_factory* editors) noexcept;
        ~slider() override;

        slider(const slider&) = delete;
        slider& operator=(const slider&) = delete;
        slider(slider&&) = delete;
        slider& operator=(slider&&) = delete;

        void set_format(dh::string fmt) noexcept;

        void bind_format(const reactive::signal<dh::string>& source) noexcept;

        void set_value_fn(dh::function<dh::string(float)> fn) noexcept;

        void set_track_only(bool track_only) noexcept;

        [[nodiscard]] float inline_value_width(const frame_ctx& ctx) const noexcept;
        void paint_inline_value(frame_ctx& ctx, rect bounds) noexcept;
        [[nodiscard]] bool on_inline_value_input(frame_ctx& ctx, rect bounds) noexcept;
        void finish_inline_edit(frame_ctx& ctx) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] float clamp_to_step(float v) const noexcept;
        void set_from_pixel(float px, frame_ctx& ctx) noexcept;

        [[nodiscard]] float value_cell_w(const frame_ctx& ctx) const noexcept;

        [[nodiscard]] rect track_rect(const frame_ctx& ctx) const noexcept;

        struct value_cells final {
            rect cell{};
            rect click{};
            float prefix_w{0.0f};
            float suffix_w{0.0f};
        };

        [[nodiscard]] value_cells value_geometry(const frame_ctx& ctx, rect value_row) const noexcept;

        [[nodiscard]] rect inline_value_row(const frame_ctx& ctx, rect bounds) const noexcept;

        void paint_value_row(frame_ctx& ctx, float v_clamped, rect value_row) noexcept;

        void begin_edit(frame_ctx& ctx) noexcept;

        void commit_edit(frame_ctx& ctx) noexcept;

        void apply_typed_chars(frame_ctx& ctx) noexcept;

        void sync_format() noexcept;

        static constexpr dh::string_view k_default_format{"{:.2f}"};
        dh::string m_label;
        dh::string m_format{k_default_format.data(), k_default_format.size()};
        const reactive::signal<dh::string>* m_format_source{nullptr};
        dh::function<dh::string(float)> m_value_fn;
        bool m_track_only{false};
        reactive::signal<float>* m_bound;
        float m_min;
        float m_max;
        float m_step;

        bool m_editing{false};
        dh::string m_edit_buf;
        float m_blink_t{0.0f};

        animation::tween m_hover_t;
        dh::unique_ptr<i_context_editor> m_context_editor;
    };

}
