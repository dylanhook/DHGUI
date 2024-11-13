// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/animation/tween.h"
#include "ui/controls/scroll_view.h"
#include "ui/core/control.h"
#include "ui/layout/stack.h"

#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"

namespace dh::ui::controls {

    class sub_tabs final : public control {
    public:
        sub_tabs() noexcept;
        ~sub_tabs() override = default;

        sub_tabs(const sub_tabs&) = delete;
        sub_tabs& operator=(const sub_tabs&) = delete;
        sub_tabs(sub_tabs&&) = delete;
        sub_tabs& operator=(sub_tabs&&) = delete;

        void make_scrollable(int viewport_h) noexcept;

        [[nodiscard]] layout::stack& add_page(dh::string name) noexcept;

        void set_active(int index) noexcept;

        [[nodiscard]] int active() const noexcept { return m_active; }

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void measure(const frame_ctx& ctx, size available) noexcept override;
        void arrange(rect r) noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        [[nodiscard]] rect strip_rect() const noexcept;
        [[nodiscard]] rect chip_rect(dh::size_t index) const noexcept;

        [[nodiscard]] rect body_rect() const noexcept;

        struct page final {
            dh::string name;

            dh::unique_ptr<control> host;
            layout::stack* body{nullptr};
            dh::unique_ptr<animation::tween> hover_t;
        };

        bool m_scrollable{false};
        int m_scroll_h{0};
        int m_active{0};
        dh::vector<page> m_pages;

        int m_subtab_h{0};
    };

}
