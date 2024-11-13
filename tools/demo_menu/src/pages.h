// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/array.h"
#include "core/dh/string.h"
#include "math/color.h"
#include "reactive/signal.h"
#include "render/image_texture.h"
#include "ui/core/control.h"
#include "ui/menu/shell.h"
#include "ui/menu/window_extent.h"

namespace dh::demo_menu {

    inline constexpr dh::size_t k_example_options = 16;

    struct checkbox_state final {
        checkbox_state() noexcept = default;
        ~checkbox_state() = default;
        checkbox_state(const checkbox_state&) = delete;
        checkbox_state& operator=(const checkbox_state&) = delete;
        checkbox_state(checkbox_state&&) = delete;
        checkbox_state& operator=(checkbox_state&&) = delete;
        reactive::signal<bool> value{false};
    };

    struct settings final {
        settings() noexcept = default;
        ~settings() = default;
        settings(const settings&) = delete;
        settings& operator=(const settings&) = delete;
        settings(settings&&) = delete;
        settings& operator=(settings&&) = delete;

        void reset_controls() noexcept;

        reactive::signal<int> open_key{VK_INSERT};
        reactive::signal<math::color> accent{math::color{184, 163, 106}};
        reactive::signal<int> window_width{ui::menu::k_window_min_width};
        reactive::signal<int> window_height{ui::menu::k_window_min_height};

        reactive::signal<bool> checkbox_1{false};
        reactive::signal<bool> checkbox_2{true};
        reactive::signal<bool> checkbox_3{true};
        reactive::signal<bool> checkbox_4{true};
        reactive::signal<bool> checkbox_5{true};
        reactive::signal<bool> checkbox_6{true};
        reactive::signal<float> slider_1{62.0f};
        reactive::signal<float> slider_2{-12.5f};
        reactive::signal<float> slider_3{0.35f};
        reactive::signal<int> combobox{1};
        reactive::signal<dh::u64> multicombobox{0b0101};
        reactive::signal<int> empty_combobox{0};
        reactive::signal<int> button_clicks{0};
        reactive::signal<bool> linked_list_open{false};
        reactive::signal<int> linked_selection{0};
        reactive::signal<dh::string> linked_filter{};

        reactive::signal<math::color> color_1{math::color{235, 90, 90}};
        reactive::signal<math::color> color_2{math::color{90, 200, 255}};
        reactive::signal<math::color> color_3{math::color{120, 220, 160, 180}};
        reactive::signal<int> listbox{2};
        reactive::signal<int> empty_listbox{0};
        dh::array<checkbox_state, 3> tab_checkboxes{};
        dh::array<checkbox_state, 20> scroll_checkboxes{};
        reactive::signal<dh::string> text_1{dh::string{"input 1"}};
        reactive::signal<dh::string> text_2{dh::string{"-12"}};
        reactive::signal<dh::string> text_3{};
        reactive::signal<bool> show_text{true};
        reactive::signal<int> submissions{0};

        reactive::signal<int> icon_selection{1};
        reactive::signal<int> horizontal_selection{1};
        reactive::signal<int> managed_selection{0};
        reactive::signal<dh::u64> managed_options{0b0011};
        reactive::signal<int> slot_selection{1};
        reactive::signal<bool> show_slider{true};
        reactive::signal<float> inline_slider{40.0f};

        reactive::signal<int> long_combobox{0};
        reactive::signal<dh::u64> long_multicombobox{(dh::u64{1} << k_example_options) - 1};
        reactive::signal<bool> show_slider_2{true};
        reactive::signal<bool> show_slider_3{false};
        reactive::signal<float> inline_slider_2{65.0f};
        reactive::signal<float> inline_slider_3{25.0f};
        dh::array<checkbox_state, 24> group_checkboxes{};
        reactive::signal<dh::string> list_filter{};
        reactive::signal<int> filtered_selection{0};
    };

    struct demo_assets final {
        demo_assets() noexcept = default;
        ~demo_assets() = default;
        demo_assets(const demo_assets&) = delete;
        demo_assets& operator=(const demo_assets&) = delete;
        demo_assets(demo_assets&&) = delete;
        demo_assets& operator=(demo_assets&&) = delete;

        [[nodiscard]] dh::expected<void, render::image::texture_failure> initialize(ID3D11Device* device) noexcept;
        static constexpr dh::u32 k_icon_side = 24;
        dh::array<render::image::image_texture, 3> icons{};
    };

    enum class page : dh::u8 { controls, lists, text, colors, layout, examples, count };

    struct landmarks final {
        landmarks() noexcept = default;
        ~landmarks() = default;
        landmarks(const landmarks&) noexcept = default;
        landmarks& operator=(const landmarks&) noexcept = default;
        landmarks(landmarks&&) noexcept = default;
        landmarks& operator=(landmarks&&) noexcept = default;
        ui::control* toggle{nullptr};
        ui::control* slider{nullptr};
        ui::control* combobox{nullptr};
        ui::control* multi_combobox{nullptr};
        ui::control* button_1{nullptr};
        ui::control* button_2{nullptr};
        ui::control* list_link{nullptr};
        ui::control* linked_choices{nullptr};
        ui::control* linked_selector{nullptr};
        ui::control* linked_search{nullptr};
        ui::control* linked_list{nullptr};
        ui::control* linked_return{nullptr};
        ui::control* text_input{nullptr};
        ui::control* color_picker{nullptr};
        ui::control* opaque_picker{nullptr};
        ui::control* managed_combobox{nullptr};
        ui::control* icon_button{nullptr};
        ui::control* horizontal_scroll{nullptr};
        ui::control* slot_strip{nullptr};
        ui::control* reveal{nullptr};
        ui::control* sub_tabs{nullptr};
        ui::control* listbox{nullptr};
        ui::control* long_combobox{nullptr};
        ui::control* long_multicombobox{nullptr};
        ui::control* reveal_toggle{nullptr};
        ui::control* revealed_slider{nullptr};
        ui::control* scroll_group{nullptr};
        ui::control* scroll_group_first{nullptr};
        ui::control* search_field{nullptr};
        ui::control* search_list{nullptr};
    };

    [[nodiscard]] ui::menu::content build_content(settings& values, demo_assets& assets, page first, landmarks& found) noexcept;
    [[nodiscard]] dh::string_view page_name(page which) noexcept;
}
