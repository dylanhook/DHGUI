// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "text_input.h"
#include "platform/clipboard.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/theme/theme.h"
#include "log/log.h"

#include <Windows.h>
#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

#include "log/log.h"
namespace dh::ui::controls {

    namespace {

        constexpr char k_mask_glyph = '*';

        constexpr float k_field_vertical_inset = 2.0f;

        constexpr dh::size_t k_max_int_digits = 10;

    }

    text_input::text_input(dh::string label, reactive::signal<dh::string>& bound, dh::size_t max_len) noexcept : text_input(dh::move(label), bound, max_len, text_domain::ascii()) {}

    text_input::text_input(dh::string label, reactive::signal<dh::string>& bound, dh::size_t max_len, text_domain domain) noexcept : m_label(dh::move(label)), m_bound(&bound), m_max_len(max_len), m_domain(domain) {}

    text_input::text_input(dh::string label, reactive::signal<int>& bound, int min, int max) noexcept : m_label(dh::move(label)), m_bound_int(&bound), m_min(min), m_max(max) {
        const dh::i64 widest = dh::max(static_cast<dh::i64>(max), -static_cast<dh::i64>(min));
        dh::size_t characters = 1;
        for (dh::i64 v = widest; v >= 10; v /= 10)
            ++characters;
        if (min < 0) ++characters;
        m_max_len = characters;
        m_domain = min < 0 ? text_domain::signed_integer() : text_domain::unsigned_integer();

        m_commit_on_blur = true;
    }

    void text_input::set_placeholder(dh::string p) noexcept {
        m_placeholder = dh::move(p);
    }

    void text_input::bind_placeholder(const reactive::signal<dh::string>& source) noexcept {
        m_placeholder_source = &source;
    }

    void text_input::on_submit(dh::function<void(const dh::string&)> callback) noexcept {
        m_on_submit = dh::move(callback);
    }

    void text_input::on_focus_lost() noexcept {
        m_character_decoder.reset();
        if (!m_edit_session) return;
        m_edit_lifetime.reset();
        m_edit_session = false;
        m_select_all = false;
        input_attachment lifetime;
        if (!lifetime.attach(*this)) return;
        // committing can destroy the control, keep the callback on the stack.
        const auto callback = m_on_submit;
        commit_edit();
        if (lifetime.owner() == nullptr || !callback) return;
        const dh::string submitted{text()};
        callback(submitted);
    }

    void text_input::begin_edit_session() noexcept {
        if (m_edit_session) return;
        m_edit_session = m_edit_lifetime.attach(*this);
        if (m_edit_session) begin_edit();
    }

    size text_input::desired_size(const frame_ctx& ctx) const noexcept {
        return {0, ctx.theme().metrics().row_h};
    }

    rect text_input::field_rect(const frame_ctx& ctx) const noexcept {
        const dh::string_view label_view{m_label.data(), m_label.size()};

        const float lab_w =
            m_label.empty() ? 0.0f : dh::min(m_rect.w * 0.4f, ctx.pnt().measure_text_w(label_view) + static_cast<float>(ctx.theme().metrics().gap_large));

        return {m_rect.x + lab_w, m_rect.y + k_field_vertical_inset, m_rect.right() - (m_rect.x + lab_w), m_rect.h - k_field_vertical_inset * 2.0f};
    }

    const dh::string& text_input::text() const noexcept {
        if (m_editing) return m_edit;
        if (m_bound_int == nullptr) return m_bound->peek_base();

        m_display.clear();
        const int value = m_bound_int->peek_base();
        if (value == 0) {
            m_display.push_back('0');
            return m_display;
        }

        const bool negative = value < 0;
        dh::i64 magnitude = negative ? -static_cast<dh::i64>(value) : value;
        char digits[k_max_int_digits]{};
        dh::size_t n = 0;
        while (magnitude > 0 && n < k_max_int_digits) {
            digits[n++] = static_cast<char>('0' + (magnitude % 10));
            magnitude /= 10;
        }
        if (negative) m_display.push_back('-');
        while (n > 0)
            m_display.push_back(digits[--n]);
        return m_display;
    }

    const dh::string& text_input::shown_text() const noexcept {
        const dh::string& source = text();
        if (m_echo == echo::shown) return source;

        const auto revision = m_editing ? m_edit_revision : (m_bound != nullptr ? m_bound->write_revision() : m_bound_int->write_revision());
        if (m_mask_source_known && m_mask_source_revision == revision && m_mask_source_editing == m_editing) return m_masked;
        m_mask_source_known = true;
        m_mask_source_revision = revision;
        m_mask_source_editing = m_editing;
        const auto scalars = platform::strings::utf8_scalar_count({source.data(), source.size()});
        if (!scalars) { ::dh::log::warn("text input cannot mask malformed UTF-8"); m_masked.clear(); return m_masked; }
        if (m_masked.size() != *scalars) {
            m_masked.clear();
            for (dh::size_t i = 0; i < *scalars; ++i) {
                m_masked.push_back(k_mask_glyph);
            }
        }
        return m_masked;
    }

    void text_input::set_text(dh::string s) noexcept {
        if (m_editing) {
            m_edit = dh::move(s);
            advance_edit_revision();
            return;
        }

        if (m_bound != nullptr) {
            m_character_revision = m_bound->write_revision() + 1;
            m_bound->set(dh::move(s));
        }
    }

    void text_input::begin_edit() noexcept {
        if (!m_commit_on_blur || m_editing) return;
        m_edit = text();
        advance_edit_revision();
        m_editing = true;
    }

    void text_input::commit_edit() noexcept {
        if (!m_editing) return;
        m_editing = false;

        if (m_bound_int != nullptr) {
            const dh::string_view typed{m_edit.data(), m_edit.size()};
            const bool negative = !typed.empty() && typed[0] == '-';

            const dh::i64 ceiling = dh::max(static_cast<dh::i64>(m_max), -static_cast<dh::i64>(m_min));
            dh::i64 magnitude = 0;
            for (const char c : typed) {
                if (c < '0' || c > '9') continue;
                magnitude = magnitude * 10 + (c - '0');
                if (magnitude > ceiling) {
                    magnitude = ceiling;
                    break;
                }
            }
            const dh::i64 parsed = negative ? -magnitude : magnitude;
            const int value = static_cast<int>(dh::clamp(parsed, static_cast<dh::i64>(m_min), static_cast<dh::i64>(m_max)));
            m_edit.clear();
            m_bound_int->set(value);
            return;
        }

        if (m_edit == m_bound->peek_base()) { m_edit.clear(); return; }
        dh::string value = dh::move(m_edit);
        m_bound->set(dh::move(value));
    }

    void text_input::discard_edit() noexcept {
        m_editing = false;
        m_edit.clear();
        advance_edit_revision();
    }

    void text_input::advance_edit_revision() noexcept {
        if (m_edit_revision == ~dh::u64{0}) dh::fail_fast("text input edit counter overflow", FAST_FAIL_INVALID_ARG);
        ++m_edit_revision;
    }

    void text_input::set_commit_on_blur(bool on) noexcept {
        m_commit_on_blur = on;

        if (!on && m_editing) commit_edit();
    }

    void text_input::set_echo(echo mode) noexcept {
        m_echo = mode;
        m_masked.clear();
        m_mask_source_known = false;
    }

    void text_input::apply_typed_chars(frame_ctx& ctx) noexcept {
        input_attachment lifetime;
        if (!lifetime.attach(*this)) return;
        if (m_typing_interval_revision != ctx.input().typing_interval_revision()) {
            m_character_decoder.reset();
            m_typing_interval_revision = ctx.input().typing_interval_revision();
        }
        if (m_bound != nullptr && m_character_revision != m_bound->write_revision()) {
            m_character_decoder.reset();
            m_character_revision = m_bound->write_revision();
        }
        const bool ctrl = ctx.input().key_down(VK_CONTROL);
        bool touched = false;

        if (ctrl && ctx.input().key_typed('A')) {
            m_character_decoder.reset();
            m_select_all = true;
            ctx.consume_keyboard();
        }
        if (ctrl && ctx.input().key_typed('V')) {
            ctx.consume_keyboard();
            m_character_decoder.reset();
            const dh::string& current = text();
            const auto before = m_select_all ? dh::string_view{} : dh::string_view{current.data(), current.size()};
            const auto available = before.size() <= m_max_len ? m_max_len - before.size() : 0;

            const auto paste_limit = is_credential_domain(m_domain) ? available : render::font_atlas::k_max_text_bytes;
            auto pasted = platform::clipboard::get_text_now(paste_limit);
            if (!pasted) {
                if (pasted.error() != platform::clipboard::clipboard_error::open_failed && pasted.error() != platform::clipboard::clipboard_error::empty)
                    ::dh::log::warn("text input paste failed ({})", platform::clipboard::to_string(pasted.error()));
            } else {
                auto edited = append_text_edit(before, {pasted->data(), pasted->size()}, m_max_len, m_domain);
                if (!edited) ::dh::log::warn("text input paste failed ({})", to_string(edited.error()));
                else if (m_select_all || !pasted->empty()) {
                    touched = true;
                    m_select_all = false;
                    ::SecureZeroMemory(pasted->data(), pasted->size());
                    set_text(dh::move(*edited));
                    if (lifetime.owner() == nullptr) return;
                }
                ::SecureZeroMemory(pasted->data(), pasted->size());
            }
            ctx.consume_keyboard();
        }
        if (ctx.input().key_typed(VK_BACK)) {
            m_character_decoder.reset();
            dh::string edited = text();
            if (m_select_all || ctrl) edited.clear();
            else if (const auto erased = erase_last_text_scalar(edited); !erased) {
                ::dh::log::warn("text input delete failed ({})", to_string(erased.error()));
                ctx.consume_keyboard();
                return;
            }
            touched = true;
            m_select_all = false;
            ctx.consume_keyboard();
            set_text(dh::move(edited));
            if (lifetime.owner() == nullptr) return;
        }

        const auto units = ctx.input().drain_typed_chars();
        if (!ctx.input().typed_chars_interval_current()) {
            m_character_decoder.reset();
            ctx.consume_keyboard();
        } else if (ctx.input().typed_chars_overflowed()) {
            m_character_decoder.reset();
            ::dh::log::warn("text input typing failed ({})", to_string(text_edit_error::unit_limit));
            ctx.consume_keyboard();
        } else if (!units.empty()) {
            ctx.consume_keyboard();
            auto characters = m_character_decoder.decode({units.data(), units.size()});
            if (!characters) ::dh::log::warn("text input typing failed ({})", to_string(characters.error()));
            else {
                if (characters->refusal()) ::dh::log::warn("text input typing failed ({})", to_string(*characters->refusal()));
                if (!characters->text().empty()) {
                    const dh::string& current = text();
                    const auto before = m_select_all ? dh::string_view{} : dh::string_view{current.data(), current.size()};
                    auto edited = append_typed_text_edit(before, {characters->text().data(), characters->text().size()}, m_max_len, m_domain);
                    if (!edited) {
                        m_character_decoder.reset();
                        ::dh::log::warn("text input typing failed ({})", to_string(edited.error()));
                    } else {
                        if (edited->refusal()) ::dh::log::warn("text input typing failed ({})", to_string(*edited->refusal()));
                        if (edited->text().size() != before.size() && ctx.input().typed_chars_interval_current()) {
                            touched = true;
                            m_select_all = false;
                            set_text(edited->take_text());
                            if (lifetime.owner() == nullptr) return;
                        }
                    }
                }
            }
            ctx.consume_keyboard();
        }

        if (touched) {
            m_blink_t = 0.0f;
        }
    }

    void text_input::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();

        const rect field = field_rect(ctx);
        const float lab_w = field.x - m_rect.x;

        const dh::string_view label_view{m_label.data(), m_label.size()};
        ctx.pnt().text_truncated({m_rect.x, ctx.pnt().text_center_y(m_rect)}, label_view, lab_w, pal.text_label);

        const bool focused = ctx.focus().is_focused(id());

        if (focused) {
            math::color halo = ctx.theme().accent_value();
            halo.a = 60;
            ctx.pnt().filled_rect({field.x - 1.0f, field.y - 1.0f, field.w + 2.0f, field.h + 2.0f}, halo);
        }
        ctx.pnt().filled_rect(field, pal.border_outer);
        const rect inside = field.inset(ctx.pnt().hairline());
        ctx.pnt().filled_rect(inside, pal.track_bot);
        if (focused) {
            const float underline = static_cast<float>(ctx.theme().metrics().active_underline_h);
            ctx.pnt().filled_rect({field.x, field.bottom() - underline, field.w, underline}, ctx.theme().accent_value());
        }

        const dh::string& s = shown_text();
        const dh::string_view s_view{s.data(), s.size()};
        const dh::string& placeholder = m_placeholder_source != nullptr ? m_placeholder_source->peek() : m_placeholder;
        const dh::string_view placeholder_view{placeholder.data(), placeholder.size()};
        const float text_pad = 8.0f;
        const float line_h = static_cast<float>(ctx.pnt().text_line_h());
        const float ty = ctx.pnt().text_center_y(field);
        const float w = s.empty() ? 0.0f : ctx.pnt().measure_text_w(s_view);

        const float caret_pad = 2.0f;
        const float available = inside.w - text_pad * 2.0f;
        dh::optional<render::text_caret> caret;
        if (focused) {
            const auto hit = ctx.pnt().trailing_text_caret(s_view);
            if (hit) caret = *hit;
        }
        float tx = inside.x + text_pad;
        const float visible_end = caret ? caret->x : w;
        if (visible_end + caret_pad > available) {
            tx = inside.right() - text_pad - visible_end - caret_pad;
        }

        const painter::clip_scope clip{ctx.pnt(), inside};
        if (s.empty() && !focused && !placeholder.empty()) {
            ctx.pnt().text({inside.x + text_pad, ty}, placeholder_view, pal.text_placeholder);
        } else {
            if (focused && m_select_all && !s.empty()) {
                math::color sel = ctx.theme().accent_value();
                sel.a = 110;
                ctx.pnt().filled_rect({tx - 1.0f, ty + 1.0f, w + 2.0f, line_h - 2.0f}, sel);
            }
            if (!s.empty()) {
                ctx.pnt().text({tx, ty}, s_view, pal.text_primary);
            }
            if (focused && !m_select_all && caret) {
                const float blink = dh::fmod(m_blink_t, 1.0f);
                if (blink < 0.5f) {
                    ctx.pnt().filled_rect({tx + caret->x + 1.0f, ty + 1.0f, 1.0f, line_h - 2.0f}, pal.text_primary);
                }
            }
        }
    }

    bool text_input::on_input(frame_ctx& ctx) noexcept {
        input_attachment lifetime;
        if (!lifetime.attach(*this)) return false;
        const point cursor = ctx.cursor();

        const bool inside = field_rect(ctx).contains(cursor);

        const bool blocked = ctx.cursor_in_popup();

        if (inside && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }
        m_blink_t += ctx.frame_dt();

        if (inside && ctx.input().key_pressed(VK_LBUTTON) && !ctx.mouse_consumed()) {
            ctx.focus().set_text_focus(*this);
            if (lifetime.owner() == nullptr) return true;
            begin_edit_session();
            ctx.consume_mouse();
            return true;
        }
        if (!inside && ctx.input().key_pressed(VK_LBUTTON) && ctx.focus().is_focused(id())) {
            begin_edit_session();
            if (!ctx.keyboard_consumed()) apply_typed_chars(ctx);
            if (lifetime.owner() == nullptr) return true;
            ctx.focus().clear_focus();
            return false;
        }

        if (!ctx.focus().is_focused(id())) {
            m_character_decoder.reset();
            m_select_all = false;

            commit_edit();
            return false;
        }

        if (ctx.input().key_pressed(VK_ESCAPE)) {
            m_character_decoder.reset();
            m_select_all = false;
            m_edit_lifetime.reset();
            m_edit_session = false;
            discard_edit();
            ctx.focus().clear_focus();
            ctx.consume_keyboard();
            return true;
        }
        if (ctx.input().key_pressed(VK_RETURN)) {
            begin_edit_session();
            if (!ctx.keyboard_consumed()) apply_typed_chars(ctx);
            if (lifetime.owner() == nullptr) return true;
            m_select_all = false;
            ctx.consume_keyboard();
            ctx.focus().clear_focus();
            return true;
        }
        begin_edit_session();
        if (!ctx.keyboard_consumed()) {
            apply_typed_chars(ctx);
        }
        return true;
    }

}
