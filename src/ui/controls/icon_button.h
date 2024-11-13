// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/signal.h"
#include "ui/animation/tween.h"
#include "ui/core/control.h"

#include "core/dh/function.h"
#include "core/dh/string.h"

struct ID3D11ShaderResourceView;

namespace dh::ui::controls {

    struct icon_source final {
        ID3D11ShaderResourceView* srv{nullptr};
        float width{0.0f};
        float height{0.0f};
    };

    enum class glyph_fit : dh::u8 { fill_height, equal_area };

    class icon_button final : public control {
    public:
        icon_button(reactive::signal<int>& selection, int index, dh::string caption, glyph_fit fit, dh::function<icon_source()> source) noexcept;
        ~icon_button() override = default;

        icon_button(const icon_button&) = delete;
        icon_button& operator=(const icon_button&) = delete;
        icon_button(icon_button&&) = delete;
        icon_button& operator=(icon_button&&) = delete;

        [[nodiscard]] static int cell_height(const frame_ctx& ctx) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        reactive::signal<int>* m_selection;
        int m_index;
        dh::string m_caption;
        glyph_fit m_fit;
        dh::function<icon_source()> m_source;
        animation::tween m_hover_t;
    };

}
