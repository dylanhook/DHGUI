// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "math/color.h"
#include "render/d3d11_renderer.h"
#include "ui/core/types.h"

#include "core/dh/string.h"

namespace dh::ui::theme {
    struct palette;
    struct sizes;
}

namespace dh::ui {

    enum class chip_state : dh::u8 { rest, active };

    class painter final {
    public:
        class clip_scope final {
        public:
            clip_scope(painter& pnt, rect r) noexcept : m_painter(&pnt) { m_painter->push_clip(r); }

            ~clip_scope() { m_painter->pop_clip(); }

            clip_scope(const clip_scope&) = delete;
            clip_scope& operator=(const clip_scope&) = delete;
            clip_scope(clip_scope&&) = delete;
            clip_scope& operator=(clip_scope&&) = delete;

        private:
            painter* m_painter;
        };

        class alpha_scope final {
        public:
            alpha_scope(painter& pnt, float a) noexcept : m_painter(&pnt), m_previous(pnt.alpha()) { m_painter->set_alpha(a); }

            ~alpha_scope() { m_painter->set_alpha(m_previous); }

            alpha_scope(const alpha_scope&) = delete;
            alpha_scope& operator=(const alpha_scope&) = delete;
            alpha_scope(alpha_scope&&) = delete;
            alpha_scope& operator=(alpha_scope&&) = delete;

        private:
            painter* m_painter;
            float m_previous;
        };

        explicit painter(render::d3d11_renderer& renderer) noexcept : m_renderer(&renderer) {}

        ~painter() = default;

        painter(const painter&) = delete;
        painter& operator=(const painter&) = delete;
        painter(painter&&) = delete;
        painter& operator=(painter&&) = delete;

        [[nodiscard]] render::d3d11_renderer& renderer() const noexcept { return *m_renderer; }

        [[nodiscard]] int viewport_w() const noexcept { return m_renderer->viewport_width(); }

        [[nodiscard]] int viewport_h() const noexcept { return m_renderer->viewport_height(); }

        [[nodiscard]] float hairline() const noexcept { return 1.0f / m_renderer->dpi_scale(); }

        void filled_rect(rect r, math::color c) noexcept;
        void outlined_rect(rect r, math::color c, float thickness = 1.0f) noexcept;

        void filled_capsule_v(rect r, math::color c) noexcept;

        void progress_ring(point center, float radius, float thickness, float fraction, math::color track, math::color fill) noexcept;

        void form_frame(rect inner, const theme::palette& pal, const theme::sizes& metrics) noexcept;

        void chiseled_panel(rect r, math::color outer, math::color inner, math::color fill) noexcept;

        void card_frame(rect r, int header_h, const theme::palette& pal, math::color accent) noexcept;

        void pill_button(rect r, const theme::palette& pal, chip_state state, float hover_t) noexcept;

        void chevron_down(point center, int base_w, math::color color, math::color shadow) noexcept;

        void chevron_right(point center, int base_h, math::color color, math::color shadow) noexcept;

        void draw_cursor(point tip, math::color fill, math::color outline) noexcept;

        void vertical_gradient(rect r, math::color top, math::color bot) noexcept;

        void horizontal_gradient(rect r, math::color left, math::color right) noexcept;

        void four_corner_gradient(rect r, math::color tl, math::color tr, math::color bl, math::color br) noexcept;

        void checkerboard(rect r, int cell_size, math::color base, math::color alternate) noexcept;

        void text(point p, dh::string_view s, math::color c) noexcept;

        static constexpr float k_text_center_bias = 2.0f;

        [[nodiscard]] float text_center_y(rect r) const noexcept;
        [[nodiscard]] float text_center_y(render::font_id font, rect r) const noexcept;

        void text_centered(rect r, dh::string_view s, math::color c) noexcept;

        void text(render::font_id font, point p, dh::string_view s, math::color c) noexcept;

        void text_truncated(point p, dh::string_view s, float max_w, math::color c) noexcept;

        [[nodiscard]] float measure_text_w(dh::string_view s) const noexcept;
        [[nodiscard]] float measure_text_w(render::font_id font, dh::string_view s) const noexcept;
        [[nodiscard]] dh::expected<render::text_caret, render::atlas_failure> trailing_text_caret(dh::string_view text) const noexcept;
        [[nodiscard]] int text_line_h() const noexcept;
        [[nodiscard]] int text_line_h(render::font_id font) const noexcept;

        [[nodiscard]] float alpha() const noexcept { return m_alpha; }

    private:
        void push_clip(rect r) noexcept;
        void pop_clip() noexcept;
        void set_alpha(float a) noexcept;

        render::d3d11_renderer* m_renderer;
        float m_alpha{1.0f};
    };

}
