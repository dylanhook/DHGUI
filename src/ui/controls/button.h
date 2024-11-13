// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/animation/tween.h"
#include "ui/core/control.h"

#include "core/dh/function.h"
#include "core/dh/optional.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"

namespace dh::ui::controls {

    class click_request final {
    public:
        [[nodiscard]] static click_request none() noexcept { return click_request{}; }

        [[nodiscard]] static click_request act(dh::function<void()> action) noexcept;

        [[nodiscard]] static click_request confirm(dh::string question, dh::function<void()> action) noexcept;

        ~click_request() = default;

        click_request(const click_request&) = delete;
        click_request& operator=(const click_request&) = delete;
        click_request(click_request&&) noexcept = default;
        click_request& operator=(click_request&&) noexcept = default;

    private:
        friend class button;

        click_request() noexcept = default;

        dh::function<void()> m_action;
        dh::optional<dh::string> m_question;
    };

    class button final : public control {
    public:
        explicit button(dh::string text) noexcept;
        ~button() override;

        button(const button&) = delete;
        button& operator=(const button&) = delete;
        button(button&&) = delete;
        button& operator=(button&&) = delete;

        void set_text(dh::string text) noexcept;

        void on_click(dh::function<void()> cb) noexcept;

        void on_click_request(dh::function<click_request()> ask) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        class confirmation;

        void click(frame_ctx& ctx) noexcept;

        dh::string m_text;
        dh::function<void()> m_on_click;
        dh::unique_ptr<confirmation> m_confirmation;
        animation::tween m_hover_t;
    };

}
