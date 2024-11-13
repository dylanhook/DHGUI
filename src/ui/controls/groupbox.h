// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "ui/controls/scroll_view.h"
#include "ui/core/control.h"
#include "ui/layout/stack.h"

namespace dh::ui::controls {

    class groupbox final : public control {
    public:
        explicit groupbox(dh::string title) noexcept;
        ~groupbox() override = default;

        groupbox(const groupbox&) = delete;
        groupbox& operator=(const groupbox&) = delete;
        groupbox(groupbox&&) = delete;
        groupbox& operator=(groupbox&&) = delete;

        void make_scrollable(int max_h) noexcept;

        void make_appended_content_scrollable() noexcept;

        void set_fixed_height(int h) noexcept { m_fixed_h = h; }

        [[nodiscard]] layout::stack& inner() noexcept;

        [[nodiscard]] const dh::string& title() const noexcept { return m_title; }

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void measure(const frame_ctx& ctx, size available) noexcept override;
        void arrange(rect r) noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        dh::string m_title;

        dh::unique_ptr<layout::stack> m_inner;
        dh::unique_ptr<scroll_view> m_scroll;

        layout::stack* m_desired_content{nullptr};

        int m_fixed_h{-1};

        int m_padding_x{0};
        int m_padding_top{0};
        int m_padding_bottom{0};
    };

}
