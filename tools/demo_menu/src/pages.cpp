// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "pages.h"

#include "core/dh/algorithm.h"
#include "core/dh/format.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"
#include "reactive/batch.h"
#include "reactive/effect.h"
#include "ui/controls/button.h"
#include "ui/controls/color_picker.h"
#include "ui/controls/combobox.h"
#include "ui/controls/groupbox.h"
#include "ui/controls/hscroll_view.h"
#include "ui/controls/icon_button.h"
#include "ui/controls/label.h"
#include "ui/controls/listbox.h"
#include "ui/controls/managed_combobox.h"
#include "ui/controls/multi_combobox.h"
#include "ui/controls/reveal.h"
#include "ui/controls/scroll_view.h"
#include "ui/controls/search_match.h"
#include "ui/controls/slider.h"
#include "ui/controls/slot_strip.h"
#include "ui/controls/sub_tabs.h"
#include "ui/controls/text_input.h"
#include "ui/controls/toggle.h"
#include "ui/core/frame_ctx.h"
#include "ui/layout/stack.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"

namespace dh::demo_menu {

    void settings::reset_controls() noexcept {
        const settings defaults;
        const reactive::batch_guard update;
        checkbox_1.set(defaults.checkbox_1.peek());
        checkbox_2.set(defaults.checkbox_2.peek());
        checkbox_3.set(defaults.checkbox_3.peek());
        checkbox_4.set(defaults.checkbox_4.peek());
        checkbox_5.set(defaults.checkbox_5.peek());
        checkbox_6.set(defaults.checkbox_6.peek());
        slider_1.set(defaults.slider_1.peek());
        slider_2.set(defaults.slider_2.peek());
        slider_3.set(defaults.slider_3.peek());
        combobox.set(defaults.combobox.peek());
        multicombobox.set(defaults.multicombobox.peek());
        color_1.set(defaults.color_1.peek());
        color_2.set(defaults.color_2.peek());
        color_3.set(defaults.color_3.peek());
        button_clicks.set(0);
        linked_filter.set(dh::string{});
        linked_list_open.set(false);
        linked_selection.set(defaults.linked_selection.peek());
    }

    dh::expected<void, render::image::texture_failure> demo_assets::initialize(ID3D11Device* device) noexcept {
        constexpr dh::size_t k_channels = 4;
        constexpr dh::u8 k_white = 255;
        constexpr int k_center = static_cast<int>(k_icon_side / 2);
        constexpr int k_radius = 8;
        dh::array<dh::u8, k_icon_side * k_icon_side * k_channels> pixels{};
        for (dh::size_t icon = 0; icon < icons.size(); ++icon) {
            for (dh::u32 y = 0; y < k_icon_side; ++y) for (dh::u32 x = 0; x < k_icon_side; ++x) {
                const int dx = static_cast<int>(x) - k_center;
                const int dy = static_cast<int>(y) - k_center;
                const int ax = dx < 0 ? -dx : dx;
                const int ay = dy < 0 ? -dy : dy;
                const bool inside = ax <= k_radius && ay <= k_radius;
                const bool ink = icon == 0 ? inside && (ax >= k_radius - 2 || ay >= k_radius - 2) : icon == 1 ? ax + ay <= k_radius : inside && (ax <= 2 || ay <= 2);
                const dh::size_t at = (static_cast<dh::size_t>(y) * k_icon_side + x) * k_channels;
                pixels[at] = pixels[at + 1] = pixels[at + 2] = k_white;
                pixels[at + 3] = ink ? k_white : 0;
            }
            auto texture = render::image::create_texture(device, pixels, k_icon_side, k_icon_side, render::image::mip_levels::single, render::image::texture_usage::immutable);
            if (!texture) return dh::unexpected{texture.error()};
            icons[icon] = dh::move(*texture);
        }
        return {};
    }

    namespace {

        constexpr const ui::i_context_editor_factory* k_no_editors = nullptr;
        constexpr dh::size_t k_option_count = 4;
        constexpr int k_list_rows = 6;
        constexpr int k_search_list_rows = 5;
        constexpr int k_link_list_rows = k_search_list_rows - 1;
        constexpr int k_scroll_height = 140;
        constexpr int k_horizontal_width = 280;
        constexpr int k_horizontal_icons = 12;

        [[nodiscard]] int gap() noexcept { return ui::theme::sizes{}.gap_large; }

        [[nodiscard]] dh::vector<dh::string> numbered(dh::string_view prefix, dh::size_t count) noexcept {
            dh::vector<dh::string> out;
            out.reserve(count);
            for (dh::size_t i = 0; i < count; ++i) out.push_back(dh::format("{} {}", prefix, i + 1));
            return out;
        }

        class column_label final : public ui::control {
        public:
            explicit column_label(dh::string text) noexcept : m_label(dh::move(text)) { adopt_child(m_label); }
            ~column_label() override = default;
            column_label(const column_label&) = delete;
            column_label& operator=(const column_label&) = delete;
            column_label(column_label&&) = delete;
            column_label& operator=(column_label&&) = delete;

            [[nodiscard]] ui::size desired_size(const ui::frame_ctx& ctx) const noexcept override {
                // use the column width so the label actually truncates.
                return {0, m_label.desired_size(ctx).h};
            }
            void arrange(ui::rect bounds) noexcept override { m_rect = bounds; m_label.arrange(bounds); }
            void paint(ui::frame_ctx& ctx) noexcept override { m_label.paint(ctx); }

        private:
            ui::controls::label m_label;
        };

        class filtered_listbox final : public ui::control {
        public:
            filtered_listbox(reactive::signal<int>& selection, reactive::signal<dh::string>& filter, int visible_rows) noexcept : m_items(numbered("item", k_example_options)), m_selection(selection), m_list(adopt_child(dh::make_unique<ui::controls::listbox>(m_cursor, dh::vector<dh::string>{}, visible_rows))), m_filter_effect([this, &filter]() noexcept {
                    const auto& query = filter.base();
                    const int selected = m_selection.base();
                    dh::vector<dh::string> matches;
                    matches.reserve(m_items.size());
                    m_indices.clear();
                    int cursor = -1;
                    for (dh::size_t i = 0; i < m_items.size(); ++i) {
                        if (!ui::controls::option_contains_ci(m_items[i], query)) continue;
                        if (selected == static_cast<int>(i)) cursor = static_cast<int>(matches.size());
                        matches.push_back(m_items[i]);
                        m_indices.push_back(static_cast<int>(i));
                    }
                    m_list->set_items(dh::move(matches));
                    m_cursor.set(cursor);
                    m_list->scroll_to_selected();
                }) {
                m_list->set_placeholder("no matches");
            }
            ~filtered_listbox() override = default;
            filtered_listbox(const filtered_listbox&) = delete;
            filtered_listbox& operator=(const filtered_listbox&) = delete;
            filtered_listbox(filtered_listbox&&) = delete;
            filtered_listbox& operator=(filtered_listbox&&) = delete;

            [[nodiscard]] ui::size desired_size(const ui::frame_ctx& ctx) const noexcept override { return m_list->desired_size(ctx); }
            void measure(const ui::frame_ctx& ctx, ui::size available) noexcept override { m_list->measure(ctx, available); }
            void arrange(ui::rect bounds) noexcept override { m_rect = bounds; m_list->arrange(bounds); }
            void paint(ui::frame_ctx& ctx) noexcept override { m_list->paint(ctx); }
            bool on_input(ui::frame_ctx& ctx) noexcept override {
                const bool handled = m_list->on_input(ctx);
                const int cursor = m_cursor.peek_base();
                if (cursor >= 0 && cursor < static_cast<int>(m_indices.size())) {
                    const int selected = m_indices[static_cast<dh::size_t>(cursor)];
                    if (m_selection.peek_base() != selected) m_selection.set(selected);
                }
                return handled;
            }
            [[nodiscard]] ui::controls::listbox& list() noexcept { return *m_list; }

        private:
            dh::vector<dh::string> m_items;
            // keep the source index, filtering changes the visible row numbers.
            reactive::signal<int>& m_selection;
            reactive::signal<int> m_cursor{-1};
            dh::unique_ptr<ui::controls::listbox> m_list;
            dh::vector<int> m_indices;
            reactive::effect m_filter_effect;
        };

        class signal_label final : public ui::control {
        public:
            signal_label(dh::string_view label, reactive::signal<int>& value) noexcept : m_label(label), m_value(&value) {}
            ~signal_label() override = default;
            signal_label(const signal_label&) = delete;
            signal_label& operator=(const signal_label&) = delete;
            signal_label(signal_label&&) = delete;
            signal_label& operator=(signal_label&&) = delete;

            [[nodiscard]] ui::size desired_size(const ui::frame_ctx& ctx) const noexcept override {
                return {0, ctx.pnt().text_line_h()};
            }
            void paint(ui::frame_ctx& ctx) noexcept override {
                constexpr dh::size_t k_text_capacity = 64;
                dh::array<char, k_text_capacity> text{};
                const int written = dh::snprintf(text.data(), text.size(), "%.*s: %d", static_cast<int>(m_label.size()), m_label.data(), m_value->peek_base());
                if (written <= 0) return;
                const auto length = dh::min(static_cast<dh::size_t>(written), text.size() - 1);
                ctx.pnt().text({m_rect.x, ctx.pnt().text_center_y(m_rect)}, {text.data(), length}, ctx.theme().colors().text_label);
            }
        private:
            dh::string_view m_label;
            reactive::signal<int>* m_value;
        };

        struct page_grid final {
            page_grid() noexcept = default;
            ~page_grid() = default;
            page_grid(const page_grid&) = delete;
            page_grid& operator=(const page_grid&) = delete;
            page_grid(page_grid&&) noexcept = default;
            page_grid& operator=(page_grid&&) noexcept = default;
            dh::unique_ptr<ui::layout::stack> root;
            dh::array<ui::layout::stack*, 2> rows{};
        };

        [[nodiscard]] page_grid grid(dh::size_t row_count) noexcept {
            page_grid page;
            page.root = dh::make_unique<ui::layout::stack>(ui::layout::orientation::vertical);
            if (!page.root) dh::alloc_failed("demo page");
            page.root->set_spacing(gap());
            page.root->set_cross_align(ui::layout::align::stretch);
            page.root->set_main_sizing(ui::layout::main_sizing::uniform);
            if (row_count > page.rows.size()) dh::fail_fast("too many demo rows", FAST_FAIL_INVALID_ARG);
            for (dh::size_t i = 0; i < row_count; ++i) {
                auto* row = page.root->emplace<ui::layout::stack>(ui::layout::orientation::horizontal);
                row->set_spacing(gap());
                row->set_cross_align(ui::layout::align::stretch);
                row->set_main_sizing(ui::layout::main_sizing::uniform);
                row->set_flex(1.0f);
                page.rows[i] = row;
            }
            return page;
        }

        [[nodiscard]] ui::controls::groupbox& group(ui::layout::stack& row, const char* title) noexcept {
            auto* box = row.emplace<ui::controls::groupbox>(dh::string{title});
            box->set_flex(1.0f);
            return *box;
        }

        [[nodiscard]] bool single_line_scalar(dh::u32 scalar) noexcept {
            constexpr dh::u32 k_first_printable = 0x20;
            constexpr dh::u32 k_control_start = 0x7F;
            constexpr dh::u32 k_control_end = 0x9F;
            constexpr dh::u32 k_line_separator = 0x2028;
            constexpr dh::u32 k_paragraph_separator = 0x2029;
            constexpr dh::u32 k_last_scalar = 0x10FFFF;
            constexpr dh::u32 k_surrogate_start = 0xD800;
            constexpr dh::u32 k_surrogate_end = 0xDFFF;
            return scalar >= k_first_printable && scalar <= k_last_scalar &&
                !(scalar >= k_surrogate_start && scalar <= k_surrogate_end) && !(scalar >= k_control_start && scalar <= k_control_end) &&
                scalar != k_line_separator && scalar != k_paragraph_separator;
        }

        [[nodiscard]] dh::unique_ptr<ui::layout::stack> controls_page(settings& values, landmarks& found) noexcept {
            auto page = grid(2);
            auto& checks = group(*page.rows[0], "checkboxes");
            auto choices = dh::make_unique<ui::controls::groupbox>("comboboxes");
            if (!choices) dh::alloc_failed("demo comboboxes");
            auto& sliders = group(*page.rows[1], "sliders");
            auto& buttons = group(*page.rows[1], "buttons");

            checks.inner().emplace<ui::controls::toggle>("checkbox 1", values.checkbox_1, k_no_editors);
            found.toggle = checks.inner().emplace<ui::controls::toggle>("checkbox 2", values.checkbox_2, k_no_editors);
            checks.inner().emplace<ui::controls::toggle>("checkbox 3", values.checkbox_3, k_no_editors)->with_color(values.color_1);
            checks.inner().emplace<ui::controls::toggle>("checkbox 4", values.checkbox_4, k_no_editors)->with_colors(values.color_2, values.color_3);
            checks.inner().emplace<ui::controls::toggle>("checkbox 5", values.checkbox_5, k_no_editors)
                ->with_status([]() noexcept { return dh::string_view{"checked"}; });
            auto* linked = checks.inner().emplace<ui::controls::toggle>("checkbox 6", values.checkbox_6, k_no_editors);
            linked->with_list_link([&values]() noexcept { values.linked_list_open.set(true); }, {});
            found.list_link = linked;

            found.slider = sliders.inner().emplace<ui::controls::slider>("slider 1", values.slider_1, 0.0f, 100.0f, 1.0f, k_no_editors);
            sliders.inner().emplace<ui::controls::slider>("slider 2", values.slider_2, -50.0f, 50.0f, 0.5f, k_no_editors)->set_format("{:.1f}");
            sliders.inner().emplace<ui::controls::slider>("slider 3", values.slider_3, 0.0f, 1.0f, 0.01f, k_no_editors)
                ->set_value_fn([](float value) noexcept { return dh::format("{}%", static_cast<int>(value * 100.0f + 0.5f)); });

            found.combobox = choices->inner().emplace<ui::controls::combobox>("combobox", values.combobox, numbered("option", k_option_count), k_no_editors);
            found.multi_combobox = choices->inner().emplace<ui::controls::multi_combobox>("multicombobox", values.multicombobox, numbered("option", k_option_count), k_no_editors);
            choices->inner().emplace<ui::controls::combobox>("combobox 2", values.empty_combobox, dh::vector<dh::string>{}, k_no_editors);
            found.linked_choices = page.rows[0]->emplace<ui::controls::reveal>([&values]() noexcept { return !values.linked_list_open(); }, dh::move(choices));
            found.linked_choices->set_flex(1.0f);

            auto selector = dh::make_unique<ui::controls::groupbox>("listbox 4");
            if (!selector) dh::alloc_failed("demo linked list");
            auto* filter = selector->inner().emplace<ui::controls::text_input>("", values.linked_filter, 64);
            filter->set_placeholder("search...");
            found.linked_search = filter;
            auto* list = &selector->inner().emplace<filtered_listbox>(values.linked_selection, values.linked_filter, k_link_list_rows)->list();
            found.linked_list = list;
            linked->with_status([list]() noexcept { return list->selected_name(); });
            auto* back = selector->inner().emplace<ui::controls::button>("return");
            back->on_click([&values]() noexcept {
                values.linked_filter.set(dh::string{});
                values.linked_list_open.set(false);
            });
            found.linked_return = back;
            found.linked_selector = page.rows[0]->emplace<ui::controls::reveal>(values.linked_list_open, dh::move(selector));
            found.linked_selector->set_flex(1.0f);

            auto* button_1 = buttons.inner().emplace<ui::controls::button>("button 1");
            button_1->on_click([&values]() noexcept { values.button_clicks.set(values.button_clicks.peek_base() + 1); });
            found.button_1 = button_1;
            auto* button_2 = buttons.inner().emplace<ui::controls::button>("button 2");
            button_2->on_click_request([&values]() noexcept {
                return ui::controls::click_request::confirm("reset controls?", [&values]() noexcept { values.reset_controls(); });
            });
            found.button_2 = button_2;
            buttons.inner().emplace<signal_label>("button clicks", values.button_clicks);
            return dh::move(page.root);
        }

        [[nodiscard]] dh::unique_ptr<ui::layout::stack> lists_page(settings& values, landmarks& found) noexcept {
            auto page = grid(2);
            found.listbox = group(*page.rows[0], "listbox 1").inner().emplace<ui::controls::listbox>(values.listbox, numbered("item", k_example_options), k_list_rows);
            group(*page.rows[1], "listbox 2").inner().emplace<ui::controls::listbox>(values.empty_listbox, dh::vector<dh::string>{}, k_list_rows);

            auto* tabs = group(*page.rows[0], "subtabs").inner().emplace<ui::controls::sub_tabs>();
            found.sub_tabs = tabs;
            for (dh::size_t i = 0; i < values.tab_checkboxes.size(); ++i) {
                auto& child = tabs->add_page(dh::format("tab {}", i + 1));
                child.emplace<ui::controls::toggle>(dh::format("checkbox {}", i + 1), values.tab_checkboxes[i].value, k_no_editors);
                child.emplace<ui::controls::label>(dh::format("label {}", i + 1));
            }
            auto* view = group(*page.rows[1], "scrollview").inner().emplace<ui::controls::scroll_view>(k_scroll_height);
            for (dh::size_t i = 0; i < values.scroll_checkboxes.size(); ++i)
                view->content().emplace<ui::controls::toggle>(dh::format("checkbox {}", i + 1), values.scroll_checkboxes[i].value, k_no_editors);
            return dh::move(page.root);
        }

        [[nodiscard]] dh::unique_ptr<ui::layout::stack> text_page(settings& values, landmarks& found) noexcept {
            auto page = grid(1);
            auto& fields = group(*page.rows[0], "textinputs");
            auto* first = fields.inner().emplace<ui::controls::text_input>("textinput 1", values.text_1, 64);
            first->on_submit([&values](const dh::string&) noexcept { values.submissions.set(values.submissions.peek_base() + 1); });
            found.text_input = first;
            fields.inner().emplace<ui::controls::text_input>("textinput 2", values.text_2, 16, ui::controls::text_domain::signed_integer());
            fields.inner().emplace<ui::controls::toggle>("checkbox 1", values.show_text, k_no_editors);
            auto third = dh::make_unique<ui::controls::text_input>("textinput 3", values.text_3, 128, ui::controls::text_domain::credential(&single_line_scalar));
            if (!third) dh::alloc_failed("demo textinput");
            third->set_placeholder("placeholder");
            fields.inner().emplace<ui::controls::reveal>(values.show_text, dh::move(third));
            fields.inner().emplace<signal_label>("submissions", values.submissions);

            auto& labels = group(*page.rows[0], "labels");
            labels.inner().emplace<ui::controls::label>("label 1");
            labels.inner().emplace<ui::controls::label>("label 2")->set_color(values.color_1.peek());
            labels.inner().emplace<ui::controls::label>("label 3 / the quick brown fox jumps over the lazy dog. the quick brown fox jumps over the lazy dog.")->set_wrap(true);
            return dh::move(page.root);
        }

        [[nodiscard]] dh::unique_ptr<ui::layout::stack> colors_page(settings& values, landmarks& found) noexcept {
            auto page = grid(1);
            auto& first = group(*page.rows[0], "colorpickers");
            found.color_picker = first.inner().emplace<ui::controls::color_picker>("colorpicker 1", values.color_1);
            auto* opaque = first.inner().emplace<ui::controls::color_picker>("colorpicker 2", values.color_2);
            opaque->set_alpha_enabled(false);
            found.opaque_picker = opaque;
            first.inner().emplace<ui::controls::color_picker>("colorpicker 3", values.color_3)->set_commit_on_release(true);
            auto& second = group(*page.rows[0], "theme");
            second.inner().emplace<ui::controls::color_picker>("colorpicker 4", values.accent);
            return dh::move(page.root);
        }

        [[nodiscard]] ui::controls::icon_source icon(demo_assets& assets, dh::size_t index) noexcept {
            const auto& texture = assets.icons[index % assets.icons.size()];
            return {texture.view.Get(), static_cast<float>(texture.width), static_cast<float>(texture.height)};
        }

        [[nodiscard]] dh::unique_ptr<ui::layout::stack> layout_page(settings& values, demo_assets& assets, landmarks& found) noexcept {
            auto page = grid(2);
            auto& icons = group(*page.rows[0], "iconbuttons");
            auto* row = icons.inner().emplace<ui::layout::stack>(ui::layout::orientation::horizontal);
            row->set_spacing(gap());
            for (dh::size_t i = 0; i < assets.icons.size(); ++i) {
                auto* button = row->emplace<ui::controls::icon_button>(values.icon_selection, static_cast<int>(i + 1), dh::format("iconbutton {}", i + 1), ui::controls::glyph_fit::equal_area, [&assets, i]() noexcept { return icon(assets, i); });
                if (i == 1) found.icon_button = button;
            }
            icons.inner().emplace<signal_label>("selection", values.icon_selection);

            auto& managed = group(*page.rows[0], "managedcombobox");
            found.managed_combobox = managed.inner().emplace<ui::controls::managed_combobox>("managedcombobox", values.managed_selection, values.managed_options, numbered("option", k_example_options), 1, nullptr);
            managed.inner().emplace<signal_label>("index", values.managed_selection);

            auto& horizontal = group(*page.rows[1], "hscrollview");
            auto* strip = horizontal.inner().emplace<ui::controls::hscroll_view>(k_horizontal_width);
            found.horizontal_scroll = strip;
            for (int i = 0; i < k_horizontal_icons; ++i)
                strip->content().emplace<ui::controls::icon_button>(values.horizontal_selection, i + 1, dh::format("iconbutton {}", i + 1), ui::controls::glyph_fit::equal_area, [&assets, i]() noexcept { return icon(assets, static_cast<dh::size_t>(i)); });
            horizontal.inner().emplace<signal_label>("selection", values.horizontal_selection);

            auto& attachments = group(*page.rows[1], "attachments");
            attachments.inner().emplace<ui::controls::label>("slotstrip");
            const auto fill = [&values](dh::size_t slot) noexcept -> dh::optional<math::color> {
                if (slot == 0) return values.color_1.peek();
                if (slot == 2) return values.color_2.peek();
                return dh::nullopt;
            };
            auto* slots = attachments.inner().emplace<ui::layout::stack>(ui::layout::orientation::horizontal);
            found.slot_strip = slots->emplace<ui::controls::slot_strip>(values.slot_selection, ui::controls::slot_strip::k_max_slots, 1, fill);
            attachments.inner().emplace<signal_label>("slot", values.slot_selection);
            auto slider = dh::make_unique<ui::controls::slider>("", values.inline_slider, 0.0f, 100.0f, 1.0f, k_no_editors);
            if (!slider) dh::alloc_failed("demo inline slider");
            slider->set_track_only(true);
            slider->set_format("{:.0f}");
            found.reveal = attachments.inner().emplace<ui::controls::toggle>("checkbox 1", values.show_slider, k_no_editors)->with_slider_value(*slider);
            attachments.inner().emplace<ui::controls::reveal>(values.show_slider, dh::move(slider));
            return dh::move(page.root);
        }

        void inline_slider(ui::layout::stack& content, const char* label, reactive::signal<bool>& enabled, reactive::signal<float>& value, landmarks& found) noexcept {
            auto slider = dh::make_unique<ui::controls::slider>("", value, 0.0f, 100.0f, 1.0f, k_no_editors);
            if (!slider) dh::alloc_failed("demo reveal slider");
            slider->set_track_only(true);
            slider->set_format("{:.0f}%");
            found.reveal_toggle = content.emplace<ui::controls::toggle>(label, enabled, k_no_editors)->with_slider_value(*slider);
            found.revealed_slider = content.emplace<ui::controls::reveal>(enabled, dh::move(slider));
        }

        [[nodiscard]] dh::unique_ptr<ui::layout::stack> examples_page(settings& values, landmarks& found) noexcept {
            auto page = grid(2);
            auto& truncated = group(*page.rows[0], "truncation");
            auto& revealed = group(*page.rows[0], "reveal");
            auto& scroll = group(*page.rows[1], "groupbox 1");
            auto& search = group(*page.rows[1], "listbox 3");

            truncated.inner().emplace<column_label>("label 4 / sample text 1, sample text 2, sample text 3, sample text 4, sample text 5");
            found.long_combobox = truncated.inner().emplace<ui::controls::combobox>("combobox 3", values.long_combobox, dh::vector<dh::string>{"option 1 / sample text 1, sample text 2, sample text 3, sample text 4", "option 2", "option 3"}, k_no_editors);
            found.long_multicombobox = truncated.inner().emplace<ui::controls::multi_combobox>("multicombobox 2", values.long_multicombobox, numbered("option", k_example_options), k_no_editors);

            inline_slider(revealed.inner(), "checkbox 1", values.show_slider_2, values.inline_slider_2, found);
            inline_slider(revealed.inner(), "checkbox 2", values.show_slider_3, values.inline_slider_3, found);

            scroll.make_scrollable(k_scroll_height);
            found.scroll_group = &scroll;
            for (dh::size_t i = 0; i < values.group_checkboxes.size(); ++i) {
                auto* checkbox = scroll.inner().emplace<ui::controls::toggle>(dh::format("checkbox {}", i + 1), values.group_checkboxes[i].value, k_no_editors);
                if (i == 0) found.scroll_group_first = checkbox;
            }

            auto* filter = search.inner().emplace<ui::controls::text_input>("", values.list_filter, 64);
            filter->set_placeholder("search...");
            found.search_field = filter;
            found.search_list = &search.inner().emplace<filtered_listbox>(values.filtered_selection, values.list_filter, k_search_list_rows)->list();
            return dh::move(page.root);
        }
    }

    dh::string_view page_name(page which) noexcept {
        switch (which) {
            case page::controls: return "controls";
            case page::lists: return "lists";
            case page::text: return "text";
            case page::colors: return "colors";
            case page::layout: return "layout";
            case page::examples: return "examples";
            case page::count: break;
        }
        return "unknown";
    }

    ui::menu::content build_content(settings& values, demo_assets& assets, page first, landmarks& found) noexcept {
        found = landmarks{};
        ui::menu::content content;
        const auto build = [&](page which) noexcept -> dh::unique_ptr<ui::layout::stack> {
            switch (which) {
                case page::controls: return controls_page(values, found);
                case page::lists: return lists_page(values, found);
                case page::text: return text_page(values, found);
                case page::colors: return colors_page(values, found);
                case page::layout: return layout_page(values, assets, found);
                case page::examples: return examples_page(values, found);
                case page::count: break;
            }
            return {};
        };
        const auto add = [&](page which) noexcept {
            auto child = build(which);
            if (!child) dh::alloc_failed("demo page");
            content.tabs.push_back(ui::menu::tab{dh::string{page_name(which)}, dh::move(child)});
        };
        add(first);
        for (dh::u8 i = 0; i < static_cast<dh::u8>(page::count); ++i) if (static_cast<page>(i) != first) add(static_cast<page>(i));
        return content;
    }
}
