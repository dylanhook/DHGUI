// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "ui/menu/view_state.h"

#include "core/dh/function.h"
#include "core/dh/memory.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"
#include "math/color.h"
#include "reactive/signal.h"
#include "render/d3d11_renderer.h"
#include "ui/overlay/overlay.h"

namespace dh::input {
    class input_state;
}

namespace dh::ui {
    class brand;
    class painter;
    class popup_layer;
    class focus_manager;

    namespace theme {
        class theme;
        struct font_roles;
    }

    namespace animation {
        class tween;
    }

    namespace layout {
        class stack;
    }
}

namespace dh::ui::menu {

    class shell_impl;

    enum class cursor_visibility : dh::u8 { follow_menu, shown };

    struct tab final {
        dh::string name;
        dh::unique_ptr<layout::stack> body;

        dh::function<bool()> visible_when{};
    };

    struct content final {
        dh::vector<tab> tabs;

        dh::unique_ptr<layout::stack> brand_page;
        dh::vector<dh::unique_ptr<overlay::i_overlay>> overlays;
    };

    struct preferences final {
        reactive::signal<int>& open_key;
        reactive::signal<math::color>& accent;

        reactive::signal<int>& window_width;
        reactive::signal<int>& window_height;
    };

    class shell final {
    public:
        shell(const brand& brand_ref, preferences prefs, theme::font_roles fonts, content hosted) noexcept;
        ~shell();

        shell(const shell&) = delete;
        shell& operator=(const shell&) = delete;
        shell(shell&&) = delete;
        shell& operator=(shell&&) = delete;

        // call prepare_input once before each render.
        void prepare_input(render::d3d11_renderer& renderer, input::input_state& input) noexcept;
        [[nodiscard]] bool render(render::d3d11_renderer& renderer, input::input_state& input) noexcept;

        void render_cursor(render::d3d11_renderer& renderer, const input::input_state& input, cursor_visibility visibility) noexcept;

        void cancel_input() noexcept;

        [[nodiscard]] bool is_open() const noexcept;

        [[nodiscard]] view_state snapshot() const noexcept;

        [[nodiscard]] bool keyboard_consumed() const noexcept;

    private:
        dh::unique_ptr<shell_impl> m_impl;
    };

}
