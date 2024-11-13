// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "math/color.h"
#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/controls/scroll_geometry.h"
#include "ui/core/context_editor.h"
#include "ui/core/control.h"
#include "ui/popup/handle.h"

#include "core/dh/array.h"
#include "core/dh/function.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"

#include "core/dh/memory.h"

namespace dh::ui::controls {

    class color_picker;
    class text_input;

    class multi_combobox final : public control {
    public:
        multi_combobox(dh::string label, reactive::signal<dh::u64>& bound, dh::vector<dh::string> options, const i_context_editor_factory* editors) noexcept;
        ~multi_combobox() override;

        multi_combobox(const multi_combobox&) = delete;
        multi_combobox& operator=(const multi_combobox&) = delete;
        multi_combobox(multi_combobox&&) = delete;
        multi_combobox& operator=(multi_combobox&&) = delete;

        multi_combobox* with_color(reactive::signal<math::color>& bound) noexcept;

        multi_combobox* with_color(reactive::signal<math::color>& bound, dh::function<bool()> shown) noexcept;

        void set_placeholder(dh::string p) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        static constexpr int k_max_visible_items = 8;

        [[nodiscard]] bool searchable() const noexcept;
        void refresh_filtered() noexcept;
        [[nodiscard]] int option_at(int filtered_row) const noexcept;

        [[nodiscard]] int visible_items() const noexcept;
        [[nodiscard]] bool overflows() const noexcept;
        void clamp_scroll() noexcept;

        [[nodiscard]] popup_scrollbar scrollbar(const frame_ctx& ctx, rect popup) const noexcept;
        [[nodiscard]] rect list_bounds(const frame_ctx& ctx, rect popup) const noexcept;
        [[nodiscard]] rect search_bounds(const frame_ctx& ctx, rect popup) const noexcept;

        void open_popup(frame_ctx& ctx) noexcept;
        void paint_popup(painter& pnt, frame_ctx& ctx) noexcept;
        bool input_popup(frame_ctx& ctx) noexcept;
        [[nodiscard]] rect compute_popup_bounds(const frame_ctx& ctx) const noexcept;

        [[nodiscard]] rect header_rect(const frame_ctx& ctx) const noexcept;

        [[nodiscard]] int label_offset(const frame_ctx& ctx) const noexcept;

        [[nodiscard]] const dh::string& joined_selection() noexcept;

        float arrange_colors(const frame_ctx& ctx) noexcept;

        [[nodiscard]] bool color_shown(dh::size_t index) const noexcept;

        dh::string m_label;
        dh::string m_placeholder{"none"};
        reactive::signal<dh::u64>* m_bound;
        dh::vector<dh::string> m_options;
        reactive::signal<dh::string> m_filter{};
        dh::string m_applied_filter{};
        dh::vector<int> m_filtered_indices{};
        dh::unique_ptr<text_input> m_search_input{};
        animation::tween m_hover_t;

        dh::vector<dh::unique_ptr<animation::tween>> m_item_check_t;

        int m_popup_hover_idx{-1};
        int m_first_visible{0};
        bool m_dragging_thumb{false};

        popup_handle m_popup{*this};
        dh::unique_ptr<i_context_editor> m_context_editor;

        dh::string m_joined;
        dh::u64 m_joined_mask{0};
        bool m_joined_valid{false};

        static constexpr dh::size_t k_max_inline_colors = 4;
        dh::array<dh::unique_ptr<color_picker>, k_max_inline_colors> m_colors{};
        dh::array<dh::function<bool()>, k_max_inline_colors> m_color_shown{};
        dh::size_t m_color_count{0};
    };

}
