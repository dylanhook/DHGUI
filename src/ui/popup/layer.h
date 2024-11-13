// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/core/types.h"

#include "core/dh/function.h"
#include "core/dh/vector.h"
#include "core/dh/memory.h"

namespace dh::ui {

    class painter;
    class frame_ctx;

    struct popup_record final {
        control_id key{};
        int z_band{0};
        rect bounds{};

        dh::function<void(painter&, frame_ctx&)> draw;

        dh::function<bool(frame_ctx&)> input;

        dh::function<void()> on_dismissed;
    };

    class popup_layer final {
    public:
        popup_layer() noexcept = default;
        ~popup_layer() = default;

        popup_layer(const popup_layer&) = delete;
        popup_layer& operator=(const popup_layer&) = delete;
        popup_layer(popup_layer&&) = delete;
        popup_layer& operator=(popup_layer&&) = delete;

        void open(popup_record rec) noexcept;

        void close(control_id key) noexcept;

        void dismiss(control_id key) noexcept;

        void close_all() noexcept;

        [[nodiscard]] bool is_open(control_id key) const noexcept;

        [[nodiscard]] bool cursor_inside_any(point cursor) const noexcept;

        [[nodiscard]] bool blocks_scroll() const noexcept;

        void set_bounds(control_id key, rect r) noexcept;

        void route_input(frame_ctx& ctx) noexcept;

        void render(painter& pnt, frame_ctx& ctx) noexcept;

    private:
        [[nodiscard]] dh::size_t find_index(control_id key) const noexcept;

        void snapshot_keys() noexcept;

        dh::vector<popup_record> m_records;

        dh::vector<control_id> m_pass_keys;
    };

}
