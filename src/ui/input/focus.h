// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/core/control.h"
#include "ui/input/attachment.h"

namespace dh::ui {

    class focus_manager final {
    public:
        focus_manager() noexcept : m_focused(this, [](void* self) noexcept { static_cast<focus_manager*>(self)->clear_focus(); }) {}
        ~focus_manager() = default;

        focus_manager(const focus_manager&) = delete;
        focus_manager& operator=(const focus_manager&) = delete;
        focus_manager(focus_manager&&) = delete;
        focus_manager& operator=(focus_manager&&) = delete;

        [[nodiscard]] bool is_focused(control_id id) const noexcept { return id && owner_id(m_focused) == id; }

        void clear_focus() noexcept {
            // drop the old focus link before its callback can change focus again.
            control* const previous = m_focused.owner();
            m_focused.reset();
            m_captures_key = false;
            if (previous != nullptr) previous->on_focus_lost();
        }

        void set_text_focus(control& owner) noexcept {
            transfer_focus(owner, focus_kind::text);
        }

        [[nodiscard]] bool wants_text_input() const noexcept { return m_focused.owner() != nullptr; }

        void set_key_capture_focus(control& owner) noexcept {
            transfer_focus(owner, focus_kind::key_capture);
        }

        [[nodiscard]] bool captures_key() const noexcept { return m_captures_key && m_focused.owner() != nullptr; }

        [[nodiscard]] bool is_captured(control_id id) const noexcept { return id && owner_id(m_captured) == id; }

        [[nodiscard]] bool any_captured() const noexcept { return m_captured.owner() != nullptr; }

        void capture(control& owner) noexcept { (void)m_captured.attach(owner); }

        void release_capture() noexcept { m_captured.reset(); }

        [[nodiscard]] control_id hovered() const noexcept { return owner_id(m_hovered); }

        void set_hovered(control& owner) noexcept { (void)m_hovered.attach(owner); }

        void clear_hovered() noexcept { m_hovered.reset(); }

    private:
        friend class scoped_hover_restore;

        enum class focus_kind { text, key_capture };

        void transfer_focus(control& owner, focus_kind kind) noexcept {
            // losing focus can delete the next control too, keep a lifetime link while it runs.
            input_attachment next;
            if (!next.attach(owner)) return;
            if (m_focused.owner() != &owner) clear_focus();
            if (next.owner() == nullptr) return;
            const bool attached = m_focused.attach(*next.owner());
            m_captures_key = attached && kind == focus_kind::key_capture;
        }

        [[nodiscard]] static control_id owner_id(const input_attachment& attachment) noexcept {
            const control* owner = attachment.owner();
            return owner != nullptr ? owner->id() : control_id{};
        }

        input_attachment m_focused;
        input_attachment m_captured;
        input_attachment m_hovered;

        bool m_captures_key{false};
    };

    class scoped_hover_restore final {
    public:
        scoped_hover_restore(focus_manager& focus, bool restore) noexcept : m_focus(focus), m_restore(restore) {
            if (restore && focus.m_hovered.owner() != nullptr) (void)m_previous.attach(*focus.m_hovered.owner());
        }

        ~scoped_hover_restore() {
            if (!m_restore) return;
            m_focus.clear_hovered();
            if (m_previous.owner() != nullptr) m_focus.set_hovered(*m_previous.owner());
        }

        scoped_hover_restore(const scoped_hover_restore&) = delete;
        scoped_hover_restore& operator=(const scoped_hover_restore&) = delete;
        scoped_hover_restore(scoped_hover_restore&&) = delete;
        scoped_hover_restore& operator=(scoped_hover_restore&&) = delete;

    private:
        focus_manager& m_focus;
        input_attachment m_previous;
        bool m_restore;
    };

}
