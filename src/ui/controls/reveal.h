// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/effect.h"
#include "reactive/signal.h"
#include "ui/core/control.h"

#include "core/dh/function.h"
#include "core/dh/memory.h"
#include "core/dh/unique_ptr.h"

#include "core/dh/utility.h"

namespace dh::ui::controls {

    class reveal final : public control {
    public:
        reveal(reactive::signal<bool>& when, dh::unique_ptr<control> child) noexcept : m_child(adopt_child(dh::move(child))), m_when([&when]() noexcept { return when.base(); }), m_visible_effect([this]() noexcept { set_visible(m_when() ? visibility::shown : visibility::hidden); }) {
            adopt_child_hint();
        }

        reveal(dh::function<bool()> when, dh::unique_ptr<control> child) noexcept : m_child(adopt_child(dh::move(child))), m_when(dh::move(when)), m_visible_effect([this]() noexcept { set_visible(m_when() ? visibility::shown : visibility::hidden); }) {
            adopt_child_hint();
        }

        ~reveal() override = default;

        reveal(const reveal&) = delete;
        reveal& operator=(const reveal&) = delete;
        reveal(reveal&&) = delete;
        reveal& operator=(reveal&&) = delete;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override { return m_child->desired_size(ctx); }

        void measure(const frame_ctx& ctx, size available) noexcept override { m_child->measure(ctx, available); }

        void arrange(rect r) noexcept override {
            m_rect = r;
            m_child->arrange(r);
        }

        void paint(frame_ctx& ctx) noexcept override { m_child->paint(ctx); }

        bool on_input(frame_ctx& ctx) noexcept override { return m_child->on_input(ctx); }

    private:
        void adopt_child_hint() noexcept {
            if (!m_child) dh::alloc_failed("ui::controls::reveal");
            // the parent lays out this wrapper, so flex and alignment have to live here.
            set_hint(m_child->hint());
        }

        dh::unique_ptr<control> m_child;

        dh::function<bool()> m_when;

        reactive::effect m_visible_effect;
    };

}
