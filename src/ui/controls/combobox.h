// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/controls/scroll_geometry.h"
#include "ui/core/context_editor.h"
#include "ui/core/control.h"
#include "ui/popup/handle.h"

#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"

namespace dh::ui::controls {

    class text_input;

    class combobox final : public control {
    public:
        combobox(dh::string label, reactive::signal<int>& bound, dh::vector<dh::string> options, const i_context_editor_factory* editors) noexcept;
        ~combobox() override;

        combobox(const combobox&) = delete;
        combobox& operator=(const combobox&) = delete;
        combobox(combobox&&) = delete;
        combobox& operator=(combobox&&) = delete;

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

        [[nodiscard]] int label_offset(const frame_ctx& ctx) const noexcept;
        [[nodiscard]] rect header_rect(const frame_ctx& ctx) const noexcept;

        dh::string m_label;
        reactive::signal<int>* m_bound;
        dh::vector<dh::string> m_options;
        reactive::signal<dh::string> m_filter{};
        dh::string m_applied_filter{};
        dh::vector<int> m_filtered_indices{};
        dh::unique_ptr<text_input> m_search_input{};
        animation::tween m_hover_t;

        int m_popup_hover_idx{-1};
        int m_first_visible{0};
        bool m_dragging_thumb{false};

        popup_handle m_popup{*this};

        dh::unique_ptr<i_context_editor> m_context_editor;
    };

}
