// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "control.h"
#include "ui/input/attachment.h"

namespace dh::ui {

    control::control() noexcept : m_id(make_control_id(this)) {}

    control::~control() {
        m_visible = false;
        cancel_input();
        while (m_first_child != nullptr)
            m_first_child->detach_from_parent();
        detach_from_parent();
    }

    void control::set_visible(visibility v) noexcept {
        const bool shown = v == visibility::shown;
        if (m_visible == shown) return;
        m_visible = shown;
        if (!shown) cancel_input();
    }

    bool control::input_available() const noexcept {
        for (const control* owner = this; owner != nullptr; owner = owner->m_parent) {
            if (!owner->m_visible) return false;
        }
        return true;
    }

    void control::adopt_child(control& child) noexcept {
        if (child.m_parent == this) return;
        child.detach_from_parent();
        child.m_parent = this;
        child.m_next_sibling = m_first_child;
        if (m_first_child != nullptr) m_first_child->m_previous_sibling = &child;
        m_first_child = &child;
        child.cancel_input();
    }

    void control::detach_from_parent() noexcept {
        if (m_previous_sibling != nullptr)
            m_previous_sibling->m_next_sibling = m_next_sibling;
        else if (m_parent != nullptr)
            m_parent->m_first_child = m_next_sibling;
        if (m_next_sibling != nullptr) m_next_sibling->m_previous_sibling = m_previous_sibling;
        m_parent = nullptr;
        m_previous_sibling = nullptr;
        m_next_sibling = nullptr;
    }

    void control::cancel_input() noexcept {
        // detach the input links first, callbacks can hide or delete other controls.
        input_attachment* pending = nullptr;
        collect_input(pending);
        while (pending != nullptr)
            pending->invalidate();
    }

    void control::collect_input(input_attachment*& pending) noexcept {
        for (control* child = m_first_child; child != nullptr; child = child->m_next_sibling)
            child->collect_input(pending);
        while (m_first_input != nullptr) {
            input_attachment* input = m_first_input;
            input->reset();
            input->link(pending, nullptr);
        }
    }

    input_attachment::~input_attachment() {
        reset();
    }

    bool input_attachment::attach(control& owner) noexcept {
        reset();
        if (!owner.input_available()) return false;
        link(owner.m_first_input, &owner);
        return true;
    }

    void input_attachment::link(input_attachment*& head, control* owner) noexcept {
        m_owner = owner;
        m_previous_link = &head;
        m_next = head;
        if (m_next != nullptr) m_next->m_previous_link = &m_next;
        head = this;
    }

    void input_attachment::reset() noexcept {
        if (m_previous_link != nullptr) *m_previous_link = m_next;
        if (m_next != nullptr) m_next->m_previous_link = m_previous_link;
        m_owner = nullptr;
        m_previous_link = nullptr;
        m_next = nullptr;
    }

    void input_attachment::invalidate() noexcept {
        reset();
        if (m_invalidated != nullptr) m_invalidated(m_context);
    }

    size control::desired_size(const frame_ctx&) const noexcept {
        return {0, 0};
    }

}
