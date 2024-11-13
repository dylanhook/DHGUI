// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "math/color.h"
#include "reactive/signal.h"
#include "render/font_table.h"

namespace dh::ui::theme {

    struct palette final {
        math::color form_outer{5, 5, 5};
        math::color form_rim{60, 60, 60};
        math::color form_dark{12, 12, 12};

        math::color form_bg{20, 20, 20};
        math::color form_grain{20, 20, 20};

        math::color rail_bg{17, 17, 17};

        math::color group_bg{17, 17, 17};
        math::color group_header_bg{17, 17, 17};
        math::color group_border_outer{0, 0, 0};
        math::color group_border_inner{45, 45, 45};

        math::color card_header_rim{48, 55, 56};
        math::color card_shadow_near{27, 32, 33};
        math::color card_shadow_far{27, 32, 33};

        math::color chip_rim_active{60, 60, 60};
        math::color chip_rim_rest{45, 45, 45};

        math::color border_outer{0, 0, 0};
        math::color border_inner{42, 42, 42};

        math::color control_top{45, 45, 45};
        math::color control_bot{32, 32, 32};
        math::color control_top_hover{55, 55, 55};
        math::color control_bot_hover{39, 39, 39};
        math::color control_pressed{23, 23, 23};

        math::color check_top{75, 75, 75};
        math::color check_bot{51, 51, 51};
        math::color check_top_hover{83, 83, 83};
        math::color check_bot_hover{58, 58, 58};

        math::color track_top{45, 45, 45};
        math::color track_bot{29, 29, 29};

        math::color list_bg{17, 17, 17};
        math::color list_row_hover{38, 38, 38};
        math::color binding_table_separator{52, 61, 62};

        math::color popup_bg{25, 25, 25};
        math::color popup_border_outer{0, 0, 0};
        math::color popup_border_inner{55, 55, 55};
        math::color popup_row_hover{40, 40, 40};

        math::color text_primary{215, 215, 215};
        math::color text_secondary{190, 190, 190};
        math::color text_label{200, 200, 200};
        math::color text_muted{130, 130, 130};
        math::color text_placeholder{90, 90, 90};
        math::color text_shadow{0, 0, 0};

        math::color checker_light{164, 176, 170};
        math::color checker_dark{124, 138, 130};

        math::color picker_cursor{237, 245, 240};

        math::color cursor_outline{12, 17, 15};
        math::color chrome_outline{0, 0, 0};
        math::color chrome_rim{45, 45, 45};

        math::color status_error{216, 115, 109};
        math::color status_warning{218, 179, 108};
        math::color status_ok{127, 185, 162};
    };

    struct sizes final {
        int window_min_w{720};
        int window_min_h{500};

        int form_bezel{5};

        int form_grain_cell{4};

        int title_accent_h{1};

        int title_bar_h{20};
        int tab_strip_h{26};

        int row_h{18};
        int control_h{20};

        int group_title_h{18};
        int group_padding_x{12};
        int group_padding_top{26};
        int group_padding_bottom{10};

        int subtab_h{22};

        int gap_normal{4};
        int gap_large{10};
        int padding_x{10};
        int content_padding{12};
        float accent_fill_mix{0.65f};

        int toggle_box{8};

        int slider_track_h{7};

        int listbox_row_h{24};
        int listbox_annotation_gap{10};

        int binding_table_header_h{20};
        int binding_table_footer_h{24};
        int binding_table_ordinal_w{58};
        int binding_table_type_w{94};
        int binding_table_key_w{58};
        int binding_table_remove_w{24};
        int binding_table_cell_inset{9};
        int binding_table_caret_inset{5};
        int binding_table_selection_stripe_w{2};

        int scroll_bar_w{4};
        int scroll_thumb_min{18};
        int hscroll_bottom_padding{0};

        int field_text_inset{8};
        int popup_row_text_inset{8};

        int popup_row_h{18};

        int popup_pad{5};

        int row_stripe_w{2};

        int active_underline_h{1};
        int text_truncate_margin{6};
        int chevron_well_w{20};
        int chevron_size{5};
        int label_row_gap{3};
        int group_title_inset{12};

        int swatch_w{18};
        int swatch_h{8};

        int key_swatch_min_w{30};
        int key_swatch_text_pad{14};
        int key_swatch_right_inset{2};
        int key_swatch_vertical_trim{3};
        int key_swatch_label_gap{8};

        int popup_z_combobox{100};
        int popup_z_picker{100};
        int popup_z_context_editor{110};
        int popup_z_picker_menu{110};

        int popup_z_confirm{100};
    };

    struct timings final {
        float fade_in{0.12f};
        float fade_out{0.10f};
        float hover{0.08f};
        float popup{0.14f};
    };

    struct font_roles final {

        render::font_id wordmark;

        render::font_id readout;
    };

    class theme final {
    public:
        theme(reactive::signal<math::color>& accent, font_roles fonts) noexcept;
        ~theme() = default;

        theme(const theme&) = delete;
        theme& operator=(const theme&) = delete;
        theme(theme&&) = delete;
        theme& operator=(theme&&) = delete;

        [[nodiscard]] const palette& colors() const noexcept { return m_palette; }

        [[nodiscard]] const sizes& metrics() const noexcept { return m_sizes; }

        [[nodiscard]] const timings& times() const noexcept { return m_timings; }

        [[nodiscard]] const font_roles& fonts() const noexcept { return m_fonts; }

        [[nodiscard]] reactive::signal<math::color>& accent_signal() noexcept { return *m_accent; }

        [[nodiscard]] math::color accent_value() const noexcept;

        [[nodiscard]] math::color accent_wash() const noexcept;

    private:
        palette m_palette{};
        sizes m_sizes{};
        timings m_timings{};
        font_roles m_fonts;

        reactive::signal<math::color>* m_accent;
    };

}
