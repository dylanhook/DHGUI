// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/optional.h"
#include "core/dh/string.h"
#include "core/dh/vector.h"
#include "math/color.h"
#include "reactive/signal.h"
#include "ui/controls/scroll_geometry.h"
#include "ui/core/control.h"

#include "core/dh/memory.h"

namespace dh::ui::controls {

    class listbox final : public control {
    public:
        struct row final {
            dh::string text{};
            dh::string annotation{};
            bool header{false};

            row() noexcept = default;
            ~row() = default;
            row(const row&) = default;
            row& operator=(const row&) = default;
            row(row&&) noexcept = default;
            row& operator=(row&&) noexcept = default;
        };

        listbox(reactive::signal<int>& bound, dh::vector<dh::string> items, int visible_rows) noexcept;
        ~listbox() override = default;

        listbox(const listbox&) = delete;
        listbox& operator=(const listbox&) = delete;
        listbox(listbox&&) = delete;
        listbox& operator=(listbox&&) = delete;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

        void set_items(dh::vector<dh::string> items) noexcept;
        void set_rows(dh::vector<row> rows) noexcept;

        void set_placeholder(dh::string text) noexcept;

        void set_row_colors(dh::vector<dh::optional<math::color>> colors) noexcept;

        [[nodiscard]] int selected() const noexcept;

        [[nodiscard]] dh::string_view selected_name() const noexcept;

        void select(dh::string_view name) noexcept;

        void scroll_to_selected() noexcept;

    private:
        [[nodiscard]] int visible_count(const frame_ctx& ctx) const noexcept;
        void clamp_scroll(const frame_ctx& ctx) noexcept;

        void rebuild_displayed() noexcept;

        [[nodiscard]] bool folded(dh::string_view header) const noexcept;
        void toggle_fold(dh::string_view header) noexcept;

        struct layout final {
            rect inside{};
            rect track{};
            scroll_metrics bar{};
            float content_w{0.0f};
            int visible{0};
        };

        [[nodiscard]] layout geometry(const frame_ctx& ctx) noexcept;

        reactive::signal<int>* m_bound{nullptr};
        dh::vector<row> m_rows{};

        dh::vector<int> m_displayed{};

        dh::vector<dh::string> m_folded{};
        dh::string m_placeholder{};

        bool m_has_headers{false};
        dh::vector<dh::optional<math::color>> m_row_colors{};
        int m_visible_rows{1};
        int m_first_visible{0};
        int m_hover_idx{-1};
        bool m_dragging_thumb{false};
        float m_drag_thumb_offset{0.0f};
    };

}
