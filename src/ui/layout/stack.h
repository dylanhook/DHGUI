// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"
#include "ui/core/control.h"
#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh::ui::layout {

    enum class orientation { vertical, horizontal };
    enum class align { start, center, end, stretch };
    enum class main_sizing { content, uniform };
    enum class spacing_mode { fixed, theme_normal, theme_label_row };

    class stack final : public control {
    public:
        explicit stack(orientation o) noexcept : m_orient(o) {}

        ~stack() override = default;

        stack(const stack&) = delete;
        stack& operator=(const stack&) = delete;
        stack(stack&&) = delete;
        stack& operator=(stack&&) = delete;

        template <typename T, typename... Args> T* emplace(Args&&... args) noexcept {
            auto p = adopt_child(dh::make_unique<T>(dh::forward<Args>(args)...));
            auto raw = p.get();
            m_children.push_back(dh::unique_ptr<control>{static_cast<control*>(p.release())});
            return raw;
        }

        template <typename T>
            requires dh::is_base_of_v<control, T>
        control* add(dh::unique_ptr<T> c) noexcept {
            auto raw = static_cast<control*>(adopt_child(dh::move(c)).release());
            m_children.push_back(dh::unique_ptr<control>{raw});
            return raw;
        }

        void set_spacing(int s) noexcept {
            m_spacing = s;
            m_spacing_mode = spacing_mode::fixed;
        }

        void set_spacing(spacing_mode mode) noexcept { m_spacing_mode = mode; }

        void set_padding(int x, int y) noexcept { set_padding(x, y, x, y); }

        void set_padding(int l, int t, int r, int b) noexcept {
            m_padding_l = l;
            m_padding_t = t;
            m_padding_r = r;
            m_padding_b = b;
        }

        void set_main_align(align a) noexcept { m_main_align = a; }

        void set_cross_align(align a) noexcept { m_cross_align = a; }

        void set_main_sizing(main_sizing sizing) noexcept { m_main_sizing = sizing; }

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;

        void measure(const frame_ctx& ctx, size available) noexcept override;
        void arrange(rect r) noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        orientation m_orient;
        dh::vector<dh::unique_ptr<control>> m_children;

        int m_spacing{0};
        int m_padding_l{0};
        int m_padding_t{0};
        int m_padding_r{0};
        int m_padding_b{0};
        int m_resolved_spacing{0};
        align m_main_align{align::start};
        align m_cross_align{align::stretch};
        main_sizing m_main_sizing{main_sizing::content};
        spacing_mode m_spacing_mode{spacing_mode::fixed};

        dh::vector<int> m_resolved_main;
        dh::vector<int> m_resolved_cross;
    };

}
