// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "log/log.h"
#include "shell.h"

#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "input/input_state.h"
#include "log/log.h"
#include "ui/animation/driver.h"
#include "ui/animation/tween.h"
#include "ui/core/brand.h"
#include "ui/core/control.h"
#include "ui/core/frame_ctx.h"
#include "ui/input/focus.h"
#include "ui/layout/stack.h"
#include "ui/menu/window_extent.h"
#include "ui/overlay/overlay.h"
#include "ui/painter/painter.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include "core/dh/array.h"
#include "core/dh/vector.h"

#include <Windows.h>
#include "core/dh/chrono.h"
#include "core/dh/algorithm.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::menu {

    namespace {

        struct tab_cell final {
            dh::string name;
            dh::unique_ptr<layout::stack> body;
            dh::unique_ptr<animation::tween> hover_t;
            dh::function<bool()> visible_when;
            bool available{true};
        };


        constexpr int k_keep_on_screen_x = 80;
        constexpr int k_keep_on_screen_y = 30;

        static_assert(k_window_max_extent == static_cast<int>(static_cast<float>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) / render::d3d11_renderer::k_min_dpi_scale), "k_window_max_extent must match the viewport limit");

    }

    class shell_impl final {
    public:
        shell_impl(const brand& brand_ref, preferences prefs, theme::font_roles fonts, content hosted) noexcept : m_brand(&brand_ref), m_open_key(&prefs.open_key), m_window_width(&prefs.window_width), m_window_height(&prefs.window_height), m_theme(prefs.accent, fonts), m_brand_page(dh::move(hosted.brand_page)), m_overlays(dh::move(hosted.overlays)) {
            take_tabs(dh::move(hosted.tabs));

            m_fade.snap(0.0f);
            apply_open_state(overlay::menu_state::open);
        }

        ~shell_impl() {
            m_popups.close_all();
        }

        shell_impl(const shell_impl&) = delete;
        shell_impl& operator=(const shell_impl&) = delete;
        shell_impl(shell_impl&&) = delete;
        shell_impl& operator=(shell_impl&&) = delete;

        void prepare_input(render::d3d11_renderer& renderer, input::input_state& input) noexcept {
            if (m_input_prepared) dh::fail_fast("menu input frame prepared twice", FAST_FAIL_INVALID_ARG);

            // advance while closed too, controls use the gap to reset their animations when shown.
            ++m_frame_seq;

            m_frame_delta = advance_frame_clock();
            process_toggle(input);
            refresh_tab_availability();
            m_input_prepared = true;
            if (m_fade.value() <= 0.0f && !m_visible) return;
            m_last_bounds = resolve_window_rect(renderer);
            painter pnt{renderer};
            frame_ctx ctx{pnt, input, m_focus, m_popups, m_theme, m_frame_delta, m_frame_seq};
            (void)latch_input_ownership(ctx, input, *m_last_bounds);
        }

        [[nodiscard]] bool render(render::d3d11_renderer& renderer, input::input_state& input) noexcept {
            if (!m_input_prepared) dh::fail_fast("menu render has no prepared input frame", FAST_FAIL_INVALID_ARG);
            m_input_prepared = false;
            const float dt = m_frame_delta;
            paint_overlays(renderer, input, dt);

            if (m_fade.value() <= 0.0f && !m_visible) {
                m_keyboard_consumed = false;
                return false;
            }

            painter pnt{renderer};

            const painter::alpha_scope fade{pnt, m_fade.value()};

            const rect window_rect = resolve_window_rect(renderer);
            m_last_bounds = window_rect;

            frame_ctx ctx{pnt, input, m_focus, m_popups, m_theme, dt, m_frame_seq};

            paint_chrome(ctx, window_rect);
            paint_content(ctx, window_rect);

            m_popups.render(pnt, ctx);

            m_focus.clear_hovered();
            if (m_visible) {
                // popups take the click before controls behind them can see it.
                m_popups.route_input(ctx);
            } else {
                ctx.consume_mouse();
                ctx.consume_keyboard();
                ctx.lock_wheel();
            }

            dispatch_input(ctx, window_rect);
            handle_resize(ctx, window_rect);
            handle_drag(ctx, window_rect);

            for (dh::size_t i = m_overlays.size(); i-- > 0;) {
                m_overlays[i]->handle_input(ctx, menu());
            }

            const bool over_chrome = latch_input_ownership(ctx, input, window_rect);

            m_keyboard_consumed = m_visible && (m_focus.wants_text_input() || ctx.keyboard_consumed());

            return m_visible && (over_chrome || input.mouse_press_claimed(VK_LBUTTON) || input.mouse_press_claimed(VK_RBUTTON));
        }

        void render_cursor(render::d3d11_renderer& renderer, const input::input_state& input, cursor_visibility visibility) noexcept {
            const bool follows_menu = visibility == cursor_visibility::follow_menu;
            if (follows_menu && m_fade.value() <= 0.0f && !m_visible) return;

            painter pnt{renderer};
            const painter::alpha_scope fade{pnt, follows_menu ? m_fade.value() : 1.0f};

            const frame_ctx ctx{pnt, input, m_focus, m_popups, m_theme, m_frame_delta, m_frame_seq};

            renderer.force_new_segment();
            pnt.draw_cursor(ctx.cursor(), m_theme.colors().text_primary, m_theme.colors().cursor_outline);
        }

        [[nodiscard]] bool is_open() const noexcept { return m_visible; }

        [[nodiscard]] view_state snapshot() const noexcept {
            view_state state;
            state.open = m_visible;
            state.bounds = m_last_bounds;
            state.accent = m_theme.accent_value();
            state.alpha = m_fade.value();
            state.colors = m_theme.colors();
            return state;
        }

        [[nodiscard]] overlay::menu_state menu() const noexcept { return m_visible ? overlay::menu_state::open : overlay::menu_state::closed; }

        [[nodiscard]] bool keyboard_consumed() const noexcept { return m_keyboard_consumed; }

        void cancel_input() noexcept {
            m_input_prepared = false;
            clear_interaction();
            m_keyboard_consumed = false;
        }

    private:
        static constexpr dh::size_t k_max_tabs = 8;

        [[nodiscard]] float advance_frame_clock() noexcept {
            constexpr float k_max_frame_dt = 0.1f;
            const auto now = dh::chrono::steady_clock::now();
            float dt = 0.0f;
            if (m_have_last_frame) {
                dt = static_cast<float>((now - m_last_frame).count()) * 1.0e-9f;
                dt = dh::clamp(dt, 0.0f, k_max_frame_dt);
            }
            m_last_frame = now;
            m_have_last_frame = true;
            return dt;
        }

        void process_toggle(input::input_state& input) noexcept {
            if (m_focus.captures_key()) return;
            if (!input.key_pressed(static_cast<unsigned int>(m_open_key->peek()))) return;

            apply_open_state(m_visible ? overlay::menu_state::closed : overlay::menu_state::open);
        }

        void apply_open_state(const overlay::menu_state state) noexcept {
            m_visible = state == overlay::menu_state::open;
            m_fade.set_target(m_visible ? 1.0f : 0.0f, m_visible ? m_theme.times().fade_in : m_theme.times().fade_out);
            if (!m_visible) {
                clear_interaction();
            }
            sync_page_visibility();
        }

        void clear_interaction() noexcept {
            m_popups.close_all();
            if (m_brand_page != nullptr) m_brand_page->cancel_input();
            for (auto& tab : m_tabs)
                tab.body->cancel_input();
            m_focus.clear_focus();
            m_focus.release_capture();
            m_focus.clear_hovered();
            m_dragging = false;
            m_resizing = false;
        }

        void sync_page_visibility() noexcept {
            if (m_brand_page != nullptr) m_brand_page->set_visible(m_visible && m_brand_page_active ? visibility::shown : visibility::hidden);
            for (dh::size_t i = 0; i < m_tabs.size(); ++i)
                m_tabs[i].body->set_visible(m_visible && !m_brand_page_active && m_tabs[i].available && static_cast<int>(i) == m_active_tab ? visibility::shown : visibility::hidden);
        }

        void refresh_tab_availability() noexcept {
            for (auto& tab : m_tabs) tab.available = !tab.visible_when || tab.visible_when();

            const bool active_available = m_active_tab >= 0 && m_active_tab < static_cast<int>(m_tabs.size()) &&
                                          m_tabs[static_cast<dh::size_t>(m_active_tab)].available;
            if (active_available) return;

            int replacement = -1;
            for (int index = m_active_tab - 1; index >= 0; --index) {
                if (!m_tabs[static_cast<dh::size_t>(index)].available) continue;
                replacement = index;
                break;
            }
            if (replacement < 0) {
                for (dh::size_t index = 0; index < m_tabs.size(); ++index) {
                    if (!m_tabs[index].available) continue;
                    replacement = static_cast<int>(index);
                    break;
                }
            }

            if (replacement == m_active_tab) return;
            clear_interaction();
            m_active_tab = replacement;
            sync_page_visibility();
        }

        void paint_overlays(render::d3d11_renderer& renderer, input::input_state& input, float dt) noexcept {
            painter overlay_pnt{renderer};
            frame_ctx overlay_ctx{overlay_pnt, input, m_focus, m_popups, m_theme, dt, m_frame_seq};
            for (const auto& one : m_overlays) {
                if (one->paint(overlay_ctx, menu())) {
                    renderer.force_new_segment();
                }
            }
        }

        [[nodiscard]] rect resolve_window_rect(render::d3d11_renderer& renderer) noexcept {
            const int vp_w = renderer.viewport_width();
            const int vp_h = renderer.viewport_height();
            const int win_w = drawn_window_extent(m_window_width->peek(), k_window_min_width, vp_w);
            const int win_h = drawn_window_extent(m_window_height->peek(), k_window_min_height, vp_h);

            const float scale = renderer.dpi_scale();
            if (scale != m_window_scale) {
                m_window_scale = scale;
                m_window_initialized = false;
                m_dragging = false;
                m_resizing = false;
            }

            if (!m_window_initialized) {
                m_window_x = (vp_w - win_w) / 2;
                m_window_y = (vp_h - win_h) / 2;
                m_window_initialized = true;
            }

            m_window_x = dh::clamp(m_window_x, 0, vp_w - k_keep_on_screen_x);
            m_window_y = dh::clamp(m_window_y, 0, vp_h - k_keep_on_screen_y);

            return {static_cast<float>(m_window_x), static_cast<float>(m_window_y), static_cast<float>(win_w), static_cast<float>(win_h)};
        }

        [[nodiscard]] bool latch_input_ownership(frame_ctx& ctx, input::input_state& input, rect window_rect) noexcept {
            const auto contains = [&](point position) noexcept {
                if (window_rect.contains(position) || m_popups.cursor_inside_any(position)) return true;
                for (const auto& one : m_overlays) {
                    if (one->contains(position)) return true;
                }
                return false;
            };
            if (m_visible) {
                for (const unsigned int button : input::k_mouse_buttons) {
                    if (const auto press = input.mouse_press_position(button); press) {
                        const float scale = ctx.pnt().renderer().dpi_scale();
                        if (contains({press->x / scale, press->y / scale}) || ctx.mouse_consumed()) (void)input.claim_mouse_press(button);
                    }
                }
            }
            return contains(ctx.cursor());
        }

        [[nodiscard]] static rect form_inner(rect window_rect, const theme::sizes& m) noexcept {
            const float bezel = static_cast<float>(m.form_bezel);
            return window_rect.inset(bezel);
        }

        [[nodiscard]] static rect title_rect(rect window_rect, const theme::sizes& m) noexcept {
            const rect inner = form_inner(window_rect, m);
            return {inner.x, inner.y + static_cast<float>(m.title_accent_h), inner.w, static_cast<float>(m.title_bar_h)};
        }

        [[nodiscard]] static rect drag_rect(rect window_rect, const theme::sizes& m) noexcept {
            const rect title = title_rect(window_rect, m);
            return {window_rect.x, window_rect.y, window_rect.w, title.bottom() - window_rect.y};
        }

        [[nodiscard]] static rect grip_rect(rect window_rect, const theme::sizes& m) noexcept {
            const rect inner = form_inner(window_rect, m);
            const float inset = static_cast<float>(m.content_padding);
            return {inner.right() - inset, inner.bottom() - inset, inset, inset};
        }

        [[nodiscard]] static rect content_rect(rect window_rect, const theme::sizes& m, float divider) noexcept {
            const rect inner = form_inner(window_rect, m);
            const float top = title_rect(window_rect, m).bottom() + static_cast<float>(m.tab_strip_h) + divider;
            return {inner.x, top, inner.w, inner.bottom() - top};
        }

        [[nodiscard]] rect brand_rect(frame_ctx& ctx, rect title) const noexcept {
            const render::font_id brand_font = ctx.theme().fonts().wordmark;
            const float x = title.x + static_cast<float>(ctx.theme().metrics().padding_x);
            const brand_halves halves = m_brand->halves();
            const float width = ctx.pnt().measure_text_w(brand_font, halves.first) + ctx.pnt().measure_text_w(brand_font, halves.second);
            return {x, title.y, width, title.h};
        }

        void take_tabs(dh::vector<tab> hosted) noexcept {
            m_tabs.clear();
            m_tabs.reserve(hosted.size());
            for (auto& one : hosted) {
                if (m_tabs.size() >= k_max_tabs) {
                    ::dh::log::warn("menu: too many tabs, hiding {} ({} max)", dh::string_view{one.name.data(), one.name.size()}, k_max_tabs);
                    continue;
                }
                auto t = dh::make_unique<animation::tween>();
                if (!t) dh::alloc_failed("ui::menu::shell");
                t->snap(0.0f);
                m_tabs.push_back({dh::move(one.name), dh::move(one.body), dh::move(t), dh::move(one.visible_when), true});
            }
        }

        void compute_tab_rects(frame_ctx& ctx, rect title, dh::array<rect, k_max_tabs>& out, dh::size_t& count) const noexcept {
            const auto& metrics = ctx.theme().metrics();
            const float pad = static_cast<float>(metrics.padding_x);
            dh::size_t visible = 0;
            for (const auto& tab : m_tabs) if (tab.available) ++visible;
            count = m_tabs.size();
            if (visible == 0) return;
            const float width = (title.w - pad * 2.0f) / static_cast<float>(visible);
            float x = title.x + pad;
            for (dh::size_t i = 0; i < count; ++i) {
                if (!m_tabs[i].available) continue;
                out[i] = {x, title.bottom(), width, static_cast<float>(metrics.tab_strip_h)};
                x += width;
            }
        }

        void paint_chrome(frame_ctx& ctx, rect window_rect) noexcept {
            auto& pnt = ctx.pnt();
            const auto& pal = ctx.theme().colors();
            const auto& metrics = ctx.theme().metrics();
            const rect inner = form_inner(window_rect, metrics);
            const rect title = title_rect(window_rect, metrics);
            const float hair = pnt.hairline();
            pnt.form_frame(inner, pal, metrics);
            pnt.filled_rect({inner.x, inner.y, inner.w, hair}, ctx.theme().accent_value());
            const auto font = ctx.theme().fonts().wordmark;
            const brand_halves halves = m_brand->halves();
            const float x = title.x + static_cast<float>(metrics.padding_x);
            const float y = pnt.text_center_y(font, title);
            pnt.text(font, {x, y}, halves.first, pal.text_primary);
            pnt.text(font, {x + pnt.measure_text_w(font, halves.first), y}, halves.second, pal.text_primary);

            dh::array<rect, k_max_tabs> cells{};
            dh::size_t count = 0;
            compute_tab_rects(ctx, title, cells, count);
            for (dh::size_t i = 0; i < count; ++i) {
                if (!m_tabs[i].available) continue;
                const rect cell = cells[i];
                const bool active = !m_brand_page_active && i == static_cast<dh::size_t>(m_active_tab);
                const float hover = m_tabs[i].hover_t->value();
                pnt.filled_rect(cell, pal.border_outer);
                const rect inside = cell.inset(hair);
                if (active) pnt.filled_rect(inside, pal.form_bg);
                else pnt.vertical_gradient(inside, math::lerp(pal.control_top, pal.control_top_hover, hover), pal.control_bot);
                if (active) pnt.filled_rect({inside.x, inside.bottom() - hair, inside.w, hair}, ctx.theme().accent_value());
                const auto& name = m_tabs[i].name;
                pnt.text_centered(cell, dh::string_view{name.data(), name.size()}, active ? pal.text_primary : pal.text_muted);
            }
        }

        void paint_content(frame_ctx& ctx, rect window_rect) noexcept {
            const auto& m = ctx.theme().metrics();

            const rect content = content_rect(window_rect, m, ctx.pnt().hairline());

            layout::stack* body = nullptr;
            if (m_brand_page_active) {
                body = m_brand_page.get();
            } else if (m_active_tab >= 0 && m_active_tab < static_cast<int>(m_tabs.size())) {
                body = m_tabs[static_cast<dh::size_t>(m_active_tab)].body.get();
            }
            if (body == nullptr) return;

            const float inset = static_cast<float>(m.content_padding);
            const rect inner{content.x + inset, content.y + inset, content.w - inset * 2.0f, content.h - inset * 2.0f};
            const size content_size{
                static_cast<int>(inner.w),
                static_cast<int>(inner.h),
            };
            body->measure(ctx, content_size);
            body->arrange(inner);
            body->paint(ctx);
        }

        void handle_resize(frame_ctx& ctx, rect window_rect) noexcept {
            const point cursor = ctx.cursor();
            const rect grip = grip_rect(window_rect, ctx.theme().metrics());

            if (m_resizing) {
                if (!ctx.input().key_down(VK_LBUTTON)) {
                    m_resizing = false;
                } else {
                    const int width = static_cast<int>(cursor.x + m_resize_grab_x) - m_window_x;
                    const int height = static_cast<int>(cursor.y + m_resize_grab_y) - m_window_y;
                    if (width != static_cast<int>(window_rect.w) || height != static_cast<int>(window_rect.h)) {
                        const int width_room = dh::min(dh::max(k_window_min_width, ctx.pnt().viewport_w() - m_window_x), k_window_max_extent);
                        const int height_room = dh::min(dh::max(k_window_min_height, ctx.pnt().viewport_h() - m_window_y), k_window_max_extent);
                        m_window_width->set(dh::clamp(width, k_window_min_width, width_room));
                        m_window_height->set(dh::clamp(height, k_window_min_height, height_room));
                    }
                    ctx.consume_mouse();
                }
            } else if (grip.contains(cursor) && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                m_resizing = true;
                m_resize_grab_x = window_rect.right() - cursor.x;
                m_resize_grab_y = window_rect.bottom() - cursor.y;
                ctx.consume_mouse();
            }
        }

        void handle_drag(frame_ctx& ctx, rect window_rect) noexcept {
            const auto& m = ctx.theme().metrics();
            const point cursor = ctx.cursor();
            const rect title_bar = drag_rect(window_rect, m);

            if (m_dragging) {
                if (!ctx.input().key_down(VK_LBUTTON)) {
                    m_dragging = false;
                } else {
                    m_window_x = static_cast<int>(cursor.x - m_drag_offset_x);
                    m_window_y = static_cast<int>(cursor.y - m_drag_offset_y);
                    ctx.consume_mouse();
                }
                return;
            }
            if (title_bar.contains(cursor) && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                m_dragging = true;
                m_drag_offset_x = cursor.x - window_rect.x;
                m_drag_offset_y = cursor.y - window_rect.y;
                ctx.consume_mouse();
            }
        }

        void dispatch_input(frame_ctx& ctx, rect window_rect) noexcept {
            const auto& m = ctx.theme().metrics();
            const point cursor = ctx.cursor();

            const rect title = title_rect(window_rect, m);

            if (m_brand_page != nullptr && brand_rect(ctx, title).contains(cursor) && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                clear_interaction();
                m_brand_page_active = !m_brand_page_active;
                sync_page_visibility();
                ctx.consume_mouse();
                return;
            }

            dh::array<rect, k_max_tabs> cells{};
            dh::size_t cell_count = 0;
            compute_tab_rects(ctx, title, cells, cell_count);

            for (dh::size_t i = 0; i < cell_count; ++i) {
                if (!m_tabs[i].available) {
                    m_tabs[i].hover_t->set_target(0.0f, ctx.theme().times().hover);
                    continue;
                }
                const bool inside = cells[i].contains(cursor);
                m_tabs[i].hover_t->set_target(inside ? 1.0f : 0.0f, ctx.theme().times().hover);
                if (inside) {
                    if (ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
                        if (m_brand_page_active || static_cast<int>(i) != m_active_tab) {
                            clear_interaction();
                            m_brand_page_active = false;
                            m_active_tab = static_cast<int>(i);
                            sync_page_visibility();
                        }
                        ctx.consume_mouse();
                        return;
                    }
                }
            }

            layout::stack* body = nullptr;
            if (m_brand_page_active) {
                body = m_brand_page.get();
            } else if (m_active_tab >= 0 && m_active_tab < static_cast<int>(m_tabs.size())) {
                body = m_tabs[static_cast<dh::size_t>(m_active_tab)].body.get();
            }
            if (body != nullptr) body->on_input(ctx);
        }

        const brand* m_brand;
        reactive::signal<int>* m_open_key;

        reactive::signal<int>* m_window_width;
        reactive::signal<int>* m_window_height;
        theme::theme m_theme;
        focus_manager m_focus;
        popup_layer m_popups;
        dh::unique_ptr<layout::stack> m_brand_page;

        dh::vector<dh::unique_ptr<overlay::i_overlay>> m_overlays;
        dh::vector<tab_cell> m_tabs;
        int m_active_tab{0};
        bool m_brand_page_active{false};
        bool m_visible{false};

        bool m_keyboard_consumed{false};
        bool m_window_initialized{false};
        dh::optional<rect> m_last_bounds{};

        float m_window_scale{0.0f};

        int m_window_x{0};
        int m_window_y{0};
        bool m_dragging{false};
        float m_drag_offset_x{0.0f};
        float m_drag_offset_y{0.0f};

        bool m_resizing{false};
        float m_resize_grab_x{0.0f};
        float m_resize_grab_y{0.0f};

        dh::chrono::steady_clock::time_point m_last_frame{};
        bool m_have_last_frame{false};
        dh::u64 m_frame_seq{0};
        float m_frame_delta{0.0f};
        bool m_input_prepared{false};

        animation::tween m_fade;
    };

    shell::shell(const brand& brand_ref, preferences prefs, theme::font_roles fonts, content hosted) noexcept : m_impl(dh::make_unique<shell_impl>(brand_ref, prefs, fonts, dh::move(hosted))) {
        if (!m_impl) dh::alloc_failed("ui::menu::shell");
    }

    shell::~shell() = default;

    void shell::prepare_input(render::d3d11_renderer& renderer, input::input_state& input) noexcept {
        m_impl->prepare_input(renderer, input);
    }

    bool shell::render(render::d3d11_renderer& renderer, input::input_state& input) noexcept {
        return m_impl->render(renderer, input);
    }

    void shell::render_cursor(render::d3d11_renderer& renderer, const input::input_state& input, cursor_visibility visibility) noexcept {
        m_impl->render_cursor(renderer, input, visibility);
    }

    void shell::cancel_input() noexcept {
        m_impl->cancel_input();
    }

    bool shell::keyboard_consumed() const noexcept {
        return m_impl->keyboard_consumed();
    }

    bool shell::is_open() const noexcept {
        return m_impl->is_open();
    }

    view_state shell::snapshot() const noexcept {
        return m_impl->snapshot();
    }

}
