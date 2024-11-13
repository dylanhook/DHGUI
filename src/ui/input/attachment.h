// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh::ui {

    class control;

    // invalidated when the attached control is hidden or destroyed.
    class input_attachment final {
    public:
        using invalidated_fn = void (*)(void*) noexcept;

        input_attachment() noexcept = default;

        input_attachment(void* context, invalidated_fn invalidated) noexcept : m_context(context), m_invalidated(invalidated) {}

        ~input_attachment();

        input_attachment(const input_attachment&) = delete;
        input_attachment& operator=(const input_attachment&) = delete;
        input_attachment(input_attachment&&) = delete;
        input_attachment& operator=(input_attachment&&) = delete;

        [[nodiscard]] bool attach(control& owner) noexcept;
        void reset() noexcept;

        [[nodiscard]] control* owner() const noexcept { return m_owner; }

    private:
        friend class control;
        void invalidate() noexcept;
        void link(input_attachment*& head, control* owner) noexcept;

        control* m_owner{nullptr};
        input_attachment** m_previous_link{nullptr};
        input_attachment* m_next{nullptr};
        void* m_context{nullptr};
        invalidated_fn m_invalidated{nullptr};
    };

}
