// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/effect.h"
#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/controls/scroll_geometry.h"
#include "ui/core/context_editor.h"
#include "ui/core/control.h"
#include "ui/popup/handle.h"

#include "core/dh/function.h"
#include "core/dh/optional.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"

#include "core/dh/memory.h"

namespace dh::ui::controls {

    class text_input;

    class managed_combobox final : public control {
    public:
        managed_combobox(dh::string label, reactive::signal<int>& selected, reactive::signal<dh::u64>& active_mask, dh::vector<dh::string> options, int pinned_count, const i_option_catalog* catalog) noexcept;
        ~managed_combobox() override;

        managed_combobox(const managed_combobox&) = delete;
        managed_combobox& operator=(const managed_combobox&) = delete;
        managed_combobox(managed_combobox&&) = delete;
        managed_combobox& operator=(managed_combobox&&) = delete;

        managed_combobox* on_select(dh::function<void(int)> cb) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        enum class row_kind { pinned, active, inactive, separator };

        struct row final {
            int option;
            row_kind kind;
        };

        static constexpr int k_no_selection = -1;

        static constexpr int k_max_visible_items = 8;

        void open_popup(frame_ctx& ctx) noexcept;
        void paint_popup(painter& pnt, frame_ctx& ctx) noexcept;
        bool input_popup(frame_ctx& ctx) noexcept;

        [[nodiscard]] rect field_rect(const frame_ctx& ctx) const noexcept;
        [[nodiscard]] rect compute_popup_bounds(const frame_ctx& ctx) noexcept;
        [[nodiscard]] bool searchable() const noexcept;
        [[nodiscard]] rect list_bounds(const frame_ctx& ctx, rect popup) const noexcept;
        [[nodiscard]] rect search_bounds(const frame_ctx& ctx, rect popup) const noexcept;

        [[nodiscard]] static float row_height(const frame_ctx& ctx, row_kind kind) noexcept;

        [[nodiscard]] float content_height(const frame_ctx& ctx) noexcept;

        [[nodiscard]] scroll_metrics bar_metrics(const frame_ctx& ctx, rect popup) noexcept;
        void clamp_scroll(const frame_ctx& ctx) noexcept;

        [[nodiscard]] const dh::vector<row>& rows() noexcept;

        void select(int option) noexcept;

        void snap_dropped_selection() noexcept;

        [[nodiscard]] bool is_overridable(int option) const noexcept;
        [[nodiscard]] dh::u64 option_bit(int option) const noexcept;

        dh::string m_label;
        reactive::signal<int>* m_selected;
        reactive::signal<dh::u64>* m_active_mask;
        dh::vector<dh::string> m_options;
        reactive::signal<dh::string> m_filter{};
        dh::string m_rows_filter{};
        dh::unique_ptr<text_input> m_search_input{};
        int m_pinned_count;
        dh::function<void(int)> m_on_select;

        animation::tween m_hover_t;

        int m_popup_hover_row{-1};
        bool m_hover_on_button{false};
        float m_scroll{0.0f};
        bool m_dragging_thumb{false};

        popup_handle m_popup{*this};

        dh::vector<row> m_rows;
        dh::u64 m_rows_mask{0};
        bool m_rows_valid{false};

        dh::optional<reactive::effect> m_selection_effect{};
    };

}
