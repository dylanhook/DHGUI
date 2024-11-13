// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/core/frame_ctx.h"
#include "ui/core/types.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/utility.h"

namespace dh::ui {

    class input_attachment;

    enum class cross_align { inherit, start, center, end, stretch };

    enum class visibility : dh::u8 { hidden, shown };

    struct layout_hint final {
        float flex{0.0f};
        int min_main{0};
        cross_align cross{cross_align::inherit};
    };

    // build and update the control tree on the same ui thread.
    class control {
    public:
        control() noexcept;
        virtual ~control();

        control(const control&) = delete;
        control& operator=(const control&) = delete;
        control(control&&) = delete;
        control& operator=(control&&) = delete;

        [[nodiscard]] control_id id() const noexcept { return m_id; }

        [[nodiscard]] virtual size desired_size(const frame_ctx& ctx) const noexcept;

        virtual void measure(const frame_ctx&  , size) noexcept {}

        virtual void arrange(rect r) noexcept { m_rect = r; }

        virtual void paint(frame_ctx& ctx) noexcept = 0;

        virtual bool on_input(frame_ctx&) noexcept { return false; }

        virtual void on_focus_lost() noexcept {}

        [[nodiscard]] rect bounds() const noexcept { return m_rect; }

        [[nodiscard]] bool visible() const noexcept { return m_visible; }

        void set_visible(visibility v) noexcept;
        [[nodiscard]] bool input_available() const noexcept;

        void cancel_input() noexcept;

        [[nodiscard]] const layout_hint& hint() const noexcept { return m_hint; }

        void set_hint(layout_hint h) noexcept { m_hint = h; }

        void set_flex(float f) noexcept { m_hint.flex = f; }

    protected:
        void adopt_child(control& child) noexcept;

        template <typename T> [[nodiscard]] dh::unique_ptr<T> adopt_child(dh::unique_ptr<T> child) noexcept {
            if (!child) dh::alloc_failed("ui::control child");
            adopt_child(*child);
            return dh::move(child);
        }

        rect m_rect{};
        layout_hint m_hint{};
        bool m_visible{true};

    private:
        friend class input_attachment;
        void collect_input(input_attachment*& pending) noexcept;
        void detach_from_parent() noexcept;

        control_id m_id;
        control* m_parent{nullptr};
        control* m_first_child{nullptr};
        control* m_previous_sibling{nullptr};
        control* m_next_sibling{nullptr};
        input_attachment* m_first_input{nullptr};
    };

}
