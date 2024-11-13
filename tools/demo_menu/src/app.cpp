// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "app.h"

#include "debug_logger.h"
#include "device.h"
#include "pages.h"
#include "png_writer.h"

#include "input/input_state.h"
#include "log/log.h"
#include "render/d3d11_renderer.h"
#include "render/font_table.h"
#include "ui/animation/driver.h"
#include "ui/core/brand.h"
#include "ui/core/control.h"
#include "ui/layout/stack.h"
#include "ui/menu/shell.h"
#include "ui/overlay/overlay.h"
#include "ui/theme/theme.h"
#include "ui/controls/slot_strip.h"
#include "ui/controls/sub_tabs.h"
#include "ui/controls/listbox.h"

#include "core/dh/array.h"
#include "core/dh/format.h"
#include "core/dh/memory.h"
#include "core/dh/unique_ptr.h"

#include <windowsx.h>

namespace dh::demo_menu {

    namespace {

        constexpr dh::u32 k_width = 940;
        constexpr dh::u32 k_height = 680;
        constexpr float k_dpi_scale = 1.0f;

        constexpr float k_capture_dt = 1.0f / 60.0f;
        constexpr int k_settle_frames = 40;

        constexpr float k_max_frame_dt = 1.0f / 30.0f;

        constexpr float k_backdrop[4] = {0.08f, 0.08f, 0.08f, 1.0f};

        constexpr dh::array<render::font_face, 2> k_fonts{{
            {"body", {L"Tahoma", 12.0f, render::font_weight::normal, render::font_render_mode::aliased}, render::font_style::dropshadow},
            {"wordmark", {L"Tahoma", 12.0f, render::font_weight::bold, render::font_render_mode::aliased}, render::font_style::dropshadow},
        }};
        static_assert(render::validate_font_table(k_fonts).has_value(), "invalid demo font table");
        constexpr render::font_id k_wordmark_font{1};
        constexpr render::font_id k_readout_font{0};

        constexpr wchar_t k_window_class[] = L"dh_gui_demo_menu";
        constexpr wchar_t k_window_title[] = L"dhgui";

        LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) noexcept {
            auto* input = reinterpret_cast<input::input_state*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE) {
                const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lp);
                ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
                return ::DefWindowProcW(window, message, wp, lp);
            }
            if (input == nullptr) return ::DefWindowProcW(window, message, wp, lp);
            switch (message) {
                case WM_MOUSEMOVE: input->on_mouse_move(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
                case WM_LBUTTONDOWN: input->on_mouse_button(VK_LBUTTON, true); return 0;
                case WM_LBUTTONUP: input->on_mouse_button(VK_LBUTTON, false); return 0;
                case WM_RBUTTONDOWN: input->on_mouse_button(VK_RBUTTON, true); return 0;
                case WM_RBUTTONUP: input->on_mouse_button(VK_RBUTTON, false); return 0;
                case WM_MOUSEWHEEL: input->on_mouse_wheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA)); return 0;
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN: {
                    const auto vk = static_cast<unsigned>(wp);

                    if ((lp & (LPARAM{1} << 30)) == 0) input->on_key_event(vk, true);
                    input->on_key_typed(vk);
                    return 0;
                }
                case WM_KEYUP:
                case WM_SYSKEYUP: input->on_key_event(static_cast<unsigned>(wp), false); return 0;
                case WM_CHAR: input->on_typed_char(static_cast<dh::u32>(wp)); return 0;
                case WM_KILLFOCUS: input->on_focus_lost(); return 0;
                case WM_SETCURSOR:

                    if (LOWORD(lp) == HTCLIENT) {
                        ::SetCursor(nullptr);
                        return TRUE;
                    }
                    break;
                case WM_CLOSE: ::PostQuitMessage(0); return 0;
                default: break;
            }
            return ::DefWindowProcW(window, message, wp, lp);
        }

        class session final {
        public:
            session() noexcept : m_renderer(m_logger.scoped(dh::string{"renderer"}), k_fonts) {}

            ~session() {
                m_shell.reset();
                m_renderer.shutdown();
                if (m_window != nullptr) ::DestroyWindow(m_window);
                log::set_active(nullptr);
            }

            session(const session&) = delete;
            session& operator=(const session&) = delete;
            session(session&&) = delete;
            session& operator=(session&&) = delete;

            [[nodiscard]] bool open(int show) noexcept {
                log::set_active(&m_logger);
                const HINSTANCE instance = ::GetModuleHandleW(nullptr);
                WNDCLASSEXW cls{};
                cls.cbSize = sizeof(cls);
                cls.lpfnWndProc = &window_proc;
                cls.hInstance = instance;
                cls.lpszClassName = k_window_class;
                cls.hCursor = nullptr;
                if (::RegisterClassExW(&cls) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
                    m_logger.error("demo: RegisterClassExW failed os={}", static_cast<unsigned>(::GetLastError()));
                    return false;
                }
                RECT frame{0, 0, static_cast<LONG>(k_width), static_cast<LONG>(k_height)};
                constexpr DWORD k_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
                ::AdjustWindowRect(&frame, k_style, FALSE);
                m_window = ::CreateWindowExW(0, k_window_class, k_window_title, k_style, CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, instance, &m_input);
                if (m_window == nullptr) {
                    m_logger.error("demo: CreateWindowExW failed os={}", static_cast<unsigned>(::GetLastError()));
                    return false;
                }
                if (const auto created = m_device.create(m_window, k_width, k_height); !created) {
                    m_logger.error("demo: device {} hr={:#x}", to_string(created.error().cause), static_cast<unsigned>(created.error().hr));
                    return false;
                }
                if (const auto loaded = m_assets.initialize(m_device.native_device()); !loaded) {
                    m_logger.error("demo: icons {} hr={:#x}", render::image::to_string(loaded.error().cause), static_cast<unsigned>(loaded.error().hr));
                    return false;
                }
                ::ShowWindow(m_window, show);
                return true;
            }

            void host(page first) noexcept {
                m_shell.reset();
                ui::menu::content content = build_content(m_values, m_assets, first, m_marks);
                m_shell = dh::make_unique<ui::menu::shell>(m_brand, ui::menu::preferences{m_values.open_key, m_values.accent, m_values.window_width, m_values.window_height}, ui::theme::font_roles{.wordmark = k_wordmark_font, .readout = k_readout_font}, dh::move(content));
                if (!m_shell) dh::alloc_failed("demo shell");
            }

            [[nodiscard]] bool frame(float dt) noexcept {
                m_input.frame_advance();
                m_device.clear(k_backdrop);
                if (const auto opened = m_renderer.begin_frame(m_device.swapchain()); !opened) return false;
                m_renderer.set_dpi_scale(k_dpi_scale);
                ui::animation::driver::tick(dt);
                m_shell->prepare_input(m_renderer, m_input);
                static_cast<void>(m_shell->render(m_renderer, m_input));
                m_shell->render_cursor(m_renderer, m_input, ui::menu::cursor_visibility::follow_menu);
                m_renderer.end_frame();
                return true;
            }

            [[nodiscard]] bool present() noexcept {
                if (const auto shown = m_device.present(); !shown) {
                    m_logger.error("demo: present {} hr={:#x}", to_string(shown.error().cause), static_cast<unsigned>(shown.error().hr));
                    return false;
                }
                return true;
            }

            [[nodiscard]] bool capture(dh::wstring_view directory, dh::string_view name) noexcept {
                auto pixels = m_device.read_back();
                if (!pixels) {
                    m_logger.error("demo: read back {} hr={:#x}", to_string(pixels.error().cause), static_cast<unsigned>(pixels.error().hr));
                    return false;
                }
                dh::wstring path{directory};
                path.push_back(L'\\');
                for (const char c : name) path.push_back(static_cast<wchar_t>(c));
                path.append(L".png");
                if (const auto written = write_png(*pixels, path); !written) {
                    m_logger.error("demo: png '{}' {} hr={:#x}", name, to_string(written.error().cause), static_cast<unsigned>(written.error().hr));
                    return false;
                }
                m_logger.info("demo: wrote {}.png", name);
                return true;
            }

            [[nodiscard]] input::input_state& input() noexcept { return m_input; }
            [[nodiscard]] const landmarks& marks() const noexcept { return m_marks; }
            [[nodiscard]] debug_logger& logger() noexcept { return m_logger; }

            [[nodiscard]] bool verify_scene(dh::string_view name) noexcept {
                const settings defaults;
                const bool valid =
                    (name != "controls_checkbox_clicked" || !m_values.checkbox_2.peek_base()) &&
                    (name != "controls_drag_slider" || m_values.slider_1.peek_base() == 50.0f) &&
                    (name != "controls_link_open" || (m_values.linked_list_open.peek_base() && m_marks.linked_selector->visible() && !m_marks.linked_choices->visible())) &&
                    (name != "controls_link_selected" || (m_values.linked_selection.peek_base() == 15 && static_cast<ui::controls::listbox*>(m_marks.linked_list)->selected_name() == "item 16")) &&
                    (name != "controls_link_returned" || (!m_values.linked_list_open.peek_base() && m_values.linked_filter.peek().empty() && m_marks.linked_choices->visible() && m_values.linked_selection.peek_base() == 15)) &&
                    (name != "controls_link_reopened" || (m_values.linked_list_open.peek_base() && static_cast<ui::controls::listbox*>(m_marks.linked_list)->selected_name() == "item 16")) &&
                    (name != "controls_link_closed" || !m_values.linked_list_open.peek_base()) &&
                    (name != "lists_item_clicked" || m_values.listbox.peek_base() == 0) &&
                    (name != "lists_subtab_clicked" || static_cast<ui::controls::sub_tabs*>(m_marks.sub_tabs)->active() == 1) &&
                    (name != "layout_managed_selected" || (m_values.managed_selection.peek_base() == 15 && (m_values.managed_options.peek_base() & (dh::u64{1} << 14)) != 0)) &&
                    (name != "controls_button_clicked" || m_values.button_clicks.peek_base() == 1) &&
                    (name != "controls_button_confirmed" || (m_values.button_clicks.peek_base() == 0 && m_values.slider_1.peek_base() == defaults.slider_1.peek_base() && m_values.checkbox_2.peek_base() && m_values.linked_selection.peek_base() == defaults.linked_selection.peek_base() && !m_values.linked_list_open.peek_base())) &&
                    (name != "text_submitted" || m_values.submissions.peek_base() == 1) &&
                    (name != "layout_icon_clicked" || m_values.icon_selection.peek_base() == 2) &&
                    (name != "layout_slot_clicked" || m_values.slot_selection.peek_base() == static_cast<int>(ui::controls::slot_strip::k_max_slots)) &&
                    (name != "layout_reveal_hidden" || !m_values.show_slider.peek_base()) &&
                    (name != "examples_reveal_shown" || (m_values.show_slider_3.peek_base() && m_marks.revealed_slider->visible())) &&
                    (name != "examples_reveal_slider" || m_values.inline_slider_3.peek_base() == 50.0f) &&
                    (name != "examples_reveal_hidden" || (!m_values.show_slider_3.peek_base() && !m_marks.revealed_slider->visible())) &&
                    (name != "examples_group_scrolled" || m_marks.scroll_group_first->bounds().y < m_marks.scroll_group->bounds().y) &&
                    (name != "examples_search_selected" || static_cast<ui::controls::listbox*>(m_marks.search_list)->selected_name() == "item 16") &&
                    (name != "examples_search_empty" || (m_values.list_filter.peek() == "no match" && static_cast<ui::controls::listbox*>(m_marks.search_list)->selected() == -1)) &&
                    (name != "examples_search_cleared" || (m_values.list_filter.peek().empty() && static_cast<ui::controls::listbox*>(m_marks.search_list)->selected_name() == "item 16"));
                if (!valid) m_logger.error("demo: check failed for scene '{}'", name);
                return valid;
            }

        private:
            debug_logger m_logger;
            device m_device;
            demo_assets m_assets;
            render::d3d11_renderer m_renderer;
            input::input_state m_input;
            settings m_values;
            ui::brand m_brand{dh::string{"dh"}, dh::string{"gui"}, nullptr, nullptr};
            landmarks m_marks{};
            dh::unique_ptr<ui::menu::shell> m_shell;
            HWND m_window{nullptr};
        };

        enum class aim : dh::u8 { center, right_edge, field, top_edge, bottom_edge, last_slot, first_row };

        [[nodiscard]] ui::point aim_at(const ui::control& target, aim where) noexcept {
            const ui::rect r = target.bounds();

            constexpr float k_swatch_inset = 10.0f;
            constexpr float k_field_depth = 0.75f;
            const float x = where == aim::right_edge ? r.right() - k_swatch_inset :
                where == aim::last_slot ? r.right() - static_cast<float>(ui::theme::sizes{}.toggle_box) * 0.5f : r.x + r.w * 0.5f;
            const float y = where == aim::field ? r.y + r.h * k_field_depth :
                where == aim::top_edge ? r.y + static_cast<float>(ui::theme::sizes{}.subtab_h) * 0.5f :
                where == aim::bottom_edge ? r.bottom() - 2.0f :
                where == aim::first_row ? r.y + static_cast<float>(ui::theme::sizes{}.listbox_row_h) * 0.5f : r.y + r.h * 0.5f;
            return {x, y};
        }

        enum class gesture : dh::u8 { none, hover, click, press, confirm, submit, manage, wheel, search_select, search_empty, search_clear };

        struct scene final {
            constexpr scene(const char* scene_name, page first_page, gesture action, ui::control* landmarks::*landmark, aim position) noexcept : name(scene_name), first(first_page), act(action), target(landmark), where(position) {}
            ~scene() = default;
            constexpr scene(const scene&) noexcept = default;
            constexpr scene& operator=(const scene&) noexcept = default;
            constexpr scene(scene&&) noexcept = default;
            constexpr scene& operator=(scene&&) noexcept = default;
            const char* name;
            page first;
            gesture act;
            ui::control* landmarks::*target;
            aim where;
        };

        constexpr scene k_scenes[]{
            {"controls", page::controls, gesture::none, nullptr, aim::center},
            {"controls_hover_toggle", page::controls, gesture::hover, &landmarks::toggle, aim::center},
            {"controls_checkbox_clicked", page::controls, gesture::click, &landmarks::toggle, aim::center},
            {"controls_drag_slider", page::controls, gesture::press, &landmarks::slider, aim::field},
            {"controls_combobox_open", page::controls, gesture::click, &landmarks::combobox, aim::field},
            {"controls_multi_combobox_open", page::controls, gesture::click, &landmarks::multi_combobox, aim::field},
            {"controls_link_open", page::controls, gesture::click, &landmarks::list_link, aim::last_slot},
            {"controls_link_selected", page::controls, gesture::search_select, &landmarks::linked_search, aim::field},
            {"controls_link_returned", page::controls, gesture::click, &landmarks::linked_return, aim::center},
            {"controls_link_reopened", page::controls, gesture::click, &landmarks::list_link, aim::last_slot},
            {"controls_link_closed", page::controls, gesture::click, &landmarks::linked_return, aim::center},
            {"controls_button_clicked", page::controls, gesture::click, &landmarks::button_1, aim::center},
            {"controls_button_confirmation", page::controls, gesture::click, &landmarks::button_2, aim::center},
            {"controls_button_confirmed", page::controls, gesture::confirm, &landmarks::button_2, aim::center},
            {"lists", page::lists, gesture::none, nullptr, aim::center},
            {"lists_item_clicked", page::lists, gesture::click, &landmarks::listbox, aim::first_row},
            {"lists_subtab_clicked", page::lists, gesture::click, &landmarks::sub_tabs, aim::top_edge},
            {"text", page::text, gesture::none, nullptr, aim::center},
            {"text_focused", page::text, gesture::click, &landmarks::text_input, aim::field},
            {"text_submitted", page::text, gesture::submit, &landmarks::text_input, aim::field},
            {"colors", page::colors, gesture::none, nullptr, aim::center},
            {"colors_picker_open", page::colors, gesture::click, &landmarks::color_picker, aim::right_edge},
            {"colors_opaque_picker_open", page::colors, gesture::click, &landmarks::opaque_picker, aim::right_edge},
            {"layout", page::layout, gesture::none, nullptr, aim::center},
            {"layout_managed_combobox_open", page::layout, gesture::click, &landmarks::managed_combobox, aim::field},
            {"layout_managed_selected", page::layout, gesture::manage, &landmarks::managed_combobox, aim::field},
            {"layout_icon_clicked", page::layout, gesture::click, &landmarks::icon_button, aim::center},
            {"layout_slot_clicked", page::layout, gesture::click, &landmarks::slot_strip, aim::last_slot},
            {"layout_horizontal_scroll", page::layout, gesture::press, &landmarks::horizontal_scroll, aim::bottom_edge},
            {"layout_reveal_hidden", page::layout, gesture::click, &landmarks::reveal, aim::center},
            {"examples", page::examples, gesture::none, nullptr, aim::center},
            {"examples_combobox_open", page::examples, gesture::click, &landmarks::long_combobox, aim::field},
            {"examples_multicombobox_open", page::examples, gesture::click, &landmarks::long_multicombobox, aim::field},
            {"examples_reveal_shown", page::examples, gesture::click, &landmarks::reveal_toggle, aim::center},
            {"examples_reveal_slider", page::examples, gesture::press, &landmarks::revealed_slider, aim::center},
            {"examples_reveal_hidden", page::examples, gesture::click, &landmarks::reveal_toggle, aim::center},
            {"examples_group_scrolled", page::examples, gesture::wheel, &landmarks::scroll_group, aim::center},
            {"examples_search_selected", page::examples, gesture::search_select, &landmarks::search_field, aim::field},
            {"examples_search_empty", page::examples, gesture::search_empty, &landmarks::search_field, aim::field},
            {"examples_search_cleared", page::examples, gesture::search_clear, &landmarks::search_field, aim::field},
            {"controls_hover_combobox", page::controls, gesture::hover, &landmarks::combobox, aim::field},
        };
        constexpr dh::size_t k_scene_count = sizeof(k_scenes) / sizeof(k_scenes[0]);

        constexpr ui::point k_parked_pointer{8.0f, static_cast<float>(k_height) - 8.0f};

        [[nodiscard]] bool settle(session& s, int frames) noexcept {
            for (int i = 0; i < frames; ++i) {
                if (!s.frame(k_capture_dt)) return false;
            }
            return true;
        }

        [[nodiscard]] bool play(session& s, const scene& sc, dh::wstring_view directory) noexcept {
            s.host(sc.first);
            s.input().on_mouse_move(static_cast<int>(k_parked_pointer.x), static_cast<int>(k_parked_pointer.y));

            if (!settle(s, k_settle_frames)) return false;
            if (sc.act != gesture::none) {
                const ui::control* target = sc.target != nullptr ? s.marks().*(sc.target) : nullptr;
                if (target == nullptr) {
                    s.logger().error("demo: missing target for scene '{}'", sc.name);
                    return false;
                }
                const ui::point p = aim_at(*target, sc.where);
                s.input().on_mouse_move(static_cast<int>(p.x), static_cast<int>(p.y));
                if (!settle(s, 2)) return false;
                const bool search = sc.act == gesture::search_select || sc.act == gesture::search_empty || sc.act == gesture::search_clear;
                if (sc.act == gesture::wheel) s.input().on_mouse_wheel(-8.0f);
                const bool click = sc.act == gesture::click || sc.act == gesture::confirm || sc.act == gesture::submit || sc.act == gesture::manage || search;
                if (click || sc.act == gesture::press) {
                    s.input().on_mouse_button(VK_LBUTTON, true);
                    if (!settle(s, 1)) return false;
                }
                if (click) {
                    s.input().on_mouse_button(VK_LBUTTON, false);
                    if (!settle(s, 2)) return false;
                    if (search) {
                        s.input().on_key_event(VK_CONTROL, true);
                        s.input().on_key_event('A', true);
                        s.input().on_key_typed('A');
                        if (!settle(s, 1)) return false;
                        s.input().on_key_event('A', false);
                        s.input().on_key_event(VK_CONTROL, false);
                        s.input().on_key_event(VK_BACK, true);
                        s.input().on_key_typed(VK_BACK);
                        if (!settle(s, 1)) return false;
                        s.input().on_key_event(VK_BACK, false);
                        const dh::string_view query = sc.act == gesture::search_select ? "ITEM 16" : sc.act == gesture::search_empty ? "no match" : "";
                        for (const char character : query) s.input().on_typed_char(static_cast<dh::u32>(character));
                        if (!settle(s, 2)) return false;
                        if (sc.act == gesture::search_select) {
                            const ui::control* list = sc.target == &landmarks::linked_search ? s.marks().linked_list : s.marks().search_list;
                            const ui::point row = aim_at(*list, aim::first_row);
                            s.input().on_mouse_move(static_cast<int>(row.x), static_cast<int>(row.y));
                            s.input().on_mouse_button(VK_LBUTTON, true);
                            if (!settle(s, 1)) return false;
                            s.input().on_mouse_button(VK_LBUTTON, false);
                            if (!settle(s, 2)) return false;
                        }
                    } else if (sc.act == gesture::confirm) {
                        const auto metrics = ui::theme::sizes{};
                        const float row = static_cast<float>(metrics.popup_row_h);
                        const float padding = static_cast<float>(metrics.popup_pad);
                        s.input().on_mouse_move(static_cast<int>(p.x), static_cast<int>(target->bounds().bottom() + padding + row * 1.5f));
                        s.input().on_mouse_button(VK_LBUTTON, true);
                        if (!settle(s, 1)) return false;
                        s.input().on_mouse_button(VK_LBUTTON, false);
                        if (!settle(s, 2)) return false;
                    } else if (sc.act == gesture::manage) {
                        for (const char character : dh::string_view{"option 16"}) s.input().on_typed_char(static_cast<dh::u32>(character));
                        if (!settle(s, 2)) return false;
                        const auto metrics = ui::theme::sizes{};
                        const auto bounds = target->bounds();
                        const float row_y = bounds.bottom() + static_cast<float>(metrics.row_h + metrics.gap_normal + metrics.popup_pad) +
                            static_cast<float>(metrics.popup_row_h) * 0.5f;
                        s.input().on_mouse_move(static_cast<int>(bounds.right()) - metrics.chevron_well_w / 2 - 1, static_cast<int>(row_y));
                        s.input().on_mouse_button(VK_LBUTTON, true);
                        if (!settle(s, 1)) return false;
                        s.input().on_mouse_button(VK_LBUTTON, false);
                        if (!settle(s, 2)) return false;
                        s.input().on_mouse_move(static_cast<int>(p.x), static_cast<int>(row_y));
                        s.input().on_mouse_button(VK_LBUTTON, true);
                        if (!settle(s, 1)) return false;
                        s.input().on_mouse_button(VK_LBUTTON, false);
                        if (!settle(s, 2)) return false;
                    } else if (sc.act == gesture::submit) {
                        s.input().on_key_event(VK_RETURN, true);
                        if (!settle(s, 1)) return false;
                        s.input().on_key_event(VK_RETURN, false);
                    }
                    s.input().on_mouse_move(static_cast<int>(k_parked_pointer.x), static_cast<int>(k_parked_pointer.y));
                }
                if (!settle(s, k_settle_frames)) return false;
            }
            if (!s.verify_scene(sc.name)) return false;
            const bool written = s.capture(directory, sc.name);
            if (sc.act == gesture::press) s.input().on_mouse_button(VK_LBUTTON, false);
            return written;
        }

    }

    int run_interactive() noexcept {
        session s;
        if (!s.open(SW_SHOWNORMAL)) return 1;
        s.host(page::controls);
        LARGE_INTEGER frequency{};
        LARGE_INTEGER last{};
        ::QueryPerformanceFrequency(&frequency);
        ::QueryPerformanceCounter(&last);
        for (;;) {
            MSG message{};
            while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) return s.logger().complaints() == 0 ? 0 : 2;
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
            LARGE_INTEGER now{};
            ::QueryPerformanceCounter(&now);
            float dt = static_cast<float>(static_cast<double>(now.QuadPart - last.QuadPart) / static_cast<double>(frequency.QuadPart));
            last = now;
            if (dt > k_max_frame_dt) dt = k_max_frame_dt;
            if (!s.frame(dt) || !s.present()) return 1;
        }
    }

    int run_capture(dh::wstring_view directory) noexcept {
        session s;

        if (!s.open(SW_HIDE)) return 1;
        for (const scene& sc : k_scenes) {
            if (!play(s, sc, directory)) return 1;
        }
        const dh::size_t complaints = s.logger().complaints();
        s.logger().info("demo: {} scenes captured, {} warnings", k_scene_count, complaints);
        return complaints == 0 ? 0 : 2;
    }

}
