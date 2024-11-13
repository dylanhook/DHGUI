// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/core/types.h"
#include "ui/input/attachment.h"

#include "core/dh/memory.h"

namespace dh::ui {

    class frame_ctx;
    class popup_layer;
    struct popup_record;

    enum class popup_slot : int {

        primary,

        picker_menu,

        context_first,
        context_second,
        context_third,
    };

    class popup_handle final {
    public:
        explicit popup_handle(control& owner, popup_slot slot = popup_slot::primary) noexcept;

        ~popup_handle();

        popup_handle(const popup_handle&) = delete;
        popup_handle& operator=(const popup_handle&) = delete;
        popup_handle(popup_handle&&) = delete;
        popup_handle& operator=(popup_handle&&) = delete;

        void open(frame_ctx& ctx, popup_record rec) noexcept;

        void close() noexcept;

        [[nodiscard]] bool is_open() const noexcept;

        void set_bounds(rect r) noexcept;

        [[nodiscard]] rect bounds() const noexcept { return m_bounds; }

    private:
        void dismiss() noexcept;

        [[nodiscard]] static constexpr control_id key_for(control_id owner, popup_slot slot) noexcept {
            constexpr dh::u64 k_slot_salt = 0x9E3779B97F4A7C15ull;
            const auto index = static_cast<dh::u64>(static_cast<unsigned>(slot));
            return control_id{owner.value ^ index * k_slot_salt};
        }

        control* m_owner;
        control_id m_key;
        input_attachment m_input;
        popup_layer* m_layer{nullptr};
        rect m_bounds{};
    };

    [[nodiscard]] int popup_item_at(rect bounds, float pad, float item_h, int count, point cursor) noexcept;

    [[nodiscard]] float list_popup_pad(const frame_ctx& ctx) noexcept;

}
