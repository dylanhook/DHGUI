// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "button.h"

#include "ui/input/focus.h"
#include "ui/painter/painter.h"
#include "ui/popup/handle.h"
#include "ui/popup/layer.h"
#include "ui/theme/theme.h"

#include <Windows.h>
#include "core/dh/memory.h"
#include "core/dh/utility.h"

namespace dh::ui::controls {

    namespace {

        enum class confirm_choice : int { confirm, cancel, count };

        constexpr int k_confirm_choices = static_cast<int>(confirm_choice::count);

        constexpr dh::string_view k_confirm_labels[static_cast<dh::size_t>(k_confirm_choices)] = {
            "confirm",
            "cancel",
        };

        constexpr int k_confirm_rows = 1 + k_confirm_choices;

    }

    click_request click_request::act(dh::function<void()> action) noexcept {
        click_request request;
        request.m_action = dh::move(action);
        return request;
    }

    click_request click_request::confirm(dh::string question, dh::function<void()> action) noexcept {
        click_request request;
        request.m_action = dh::move(action);

        if (request.m_action) request.m_question = dh::move(question);
        return request;
    }

    class button::confirmation final {
    public:
        confirmation(control& owner, const rect& anchor, dh::function<click_request()> ask) noexcept : m_anchor(&anchor), m_ask(dh::move(ask)), m_popup(owner) {}
        ~confirmation() = default;

        confirmation(const confirmation&) = delete;
        confirmation& operator=(const confirmation&) = delete;
        confirmation(confirmation&&) = delete;
        confirmation& operator=(confirmation&&) = delete;

        void click(frame_ctx& ctx) noexcept {
            if (!m_ask) return;
            click_request request = m_ask();
            if (!request.m_action) return;
            if (!request.m_question) {
                request.m_action();
                return;
            }
            open(ctx, dh::move(*request.m_question), dh::move(request.m_action));
        }

        [[nodiscard]] bool is_open() const noexcept { return m_popup.is_open(); }

    private:
        void open(frame_ctx& ctx, dh::string question, dh::function<void()> action) noexcept {
            m_question = dh::move(question);
            m_pending = dh::move(action);
            m_hover = -1;

            popup_record rec;
            rec.z_band = ctx.theme().metrics().popup_z_confirm;
            rec.bounds = bounds(ctx);
            rec.draw = [this](painter& pnt, frame_ctx& fctx) noexcept { paint(pnt, fctx); };
            rec.input = [this](frame_ctx& fctx) noexcept { return input(fctx); };

            rec.on_dismissed = [this]() noexcept { forget(); };
            m_popup.open(ctx, static_cast<popup_record&&>(rec));
        }

        void close() noexcept {
            m_popup.close();
            forget();
        }

        void forget() noexcept {
            m_pending = dh::function<void()>{};
            m_question.clear();
            m_hover = -1;
        }

        [[nodiscard]] rect bounds(const frame_ctx& ctx) const noexcept {
            const rect anchor = *m_anchor;
            const float row_h = static_cast<float>(ctx.theme().metrics().popup_row_h);
            const float h = list_popup_pad(ctx) * 2.0f + row_h * static_cast<float>(k_confirm_rows);
            const float vp_h = static_cast<float>(ctx.pnt().viewport_h());
            float y = anchor.bottom();
            if (y + h > vp_h) {
                y = anchor.y - h;
                if (y < 0.0f) y = 0.0f;
            }
            return {anchor.x, y, anchor.w, h};
        }

        void paint(painter& pnt, frame_ctx& ctx) noexcept {
            const auto& pal = ctx.theme().colors();
            const auto& m = ctx.theme().metrics();
            const rect r = m_popup.bounds();
            pnt.chiseled_panel(r, pal.popup_border_outer, pal.popup_border_inner, pal.popup_bg);

            const float hair = pnt.hairline();
            const float row_h = static_cast<float>(m.popup_row_h);
            const float text_inset = static_cast<float>(m.popup_row_text_inset);
            const float text_w = r.w - hair * 2.0f - text_inset - static_cast<float>(m.text_truncate_margin);

            const rect question{r.x + hair, r.y + list_popup_pad(ctx), r.w - hair * 2.0f, row_h};
            pnt.text_truncated({question.x + text_inset, pnt.text_center_y(question)}, dh::string_view{m_question.data(), m_question.size()}, text_w, pal.text_label);

            for (int i = 0; i < k_confirm_choices; ++i) {
                const rect row{question.x, question.bottom() + row_h * static_cast<float>(i), question.w, row_h};
                const bool hov = m_hover == i;
                if (hov) {
                    pnt.filled_rect(row, ctx.theme().accent_wash());
                    pnt.filled_rect({row.x, row.y, static_cast<float>(m.row_stripe_w), row.h}, ctx.theme().accent_value());
                }
                pnt.text({row.x + text_inset, pnt.text_center_y(row)}, k_confirm_labels[static_cast<dh::size_t>(i)], hov ? pal.text_primary : pal.text_secondary);
            }

            pnt.filled_rect({question.x + text_inset, question.bottom(), question.w - text_inset * 2.0f, hair}, pal.popup_border_inner);
        }

        bool input(frame_ctx& ctx) noexcept {
            if (ctx.input().key_pressed(VK_ESCAPE)) {
                ctx.consume_keyboard();
                close();
                return true;
            }

            m_popup.set_bounds(bounds(ctx));
            const rect r = m_popup.bounds();
            const point cursor = ctx.cursor();
            const float row_h = static_cast<float>(ctx.theme().metrics().popup_row_h);

            m_hover = popup_item_at(r, list_popup_pad(ctx) + row_h, row_h, k_confirm_choices, cursor);

            if (ctx.input().key_pressed(VK_LBUTTON) && m_hover >= 0) {
                ctx.consume_mouse();
                switch (static_cast<confirm_choice>(m_hover)) {
                    case confirm_choice::confirm: {
                        dh::function<void()> action = dh::move(m_pending);
                        close();
                        if (action) action();
                        break;
                    }
                    case confirm_choice::cancel:
                        close();
                        break;

                    case confirm_choice::count:
                        break;
                }
                return true;
            }
            return r.contains(cursor);
        }

        const rect* m_anchor;
        dh::function<click_request()> m_ask;

        dh::function<void()> m_pending;
        dh::string m_question;
        int m_hover{-1};

        popup_handle m_popup;
    };

    button::button(dh::string text) noexcept : m_text(dh::move(text)) {
        m_hover_t.snap(0.0f);
    }

    button::~button() = default;

    void button::set_text(dh::string text) noexcept {
        m_text = dh::move(text);
    }

    void button::on_click(dh::function<void()> cb) noexcept {
        m_confirmation.reset();
        m_on_click = dh::move(cb);
    }

    void button::on_click_request(dh::function<click_request()> ask) noexcept {
        m_on_click = dh::function<void()>{};
        m_confirmation = dh::make_unique<confirmation>(*this, m_rect, dh::move(ask));

        if (!m_confirmation) dh::alloc_failed("ui::controls::button");
    }

    size button::desired_size(const frame_ctx& ctx) const noexcept {
        const float w = ctx.pnt().measure_text_w(dh::string_view{m_text.data(), m_text.size()}) + static_cast<float>(ctx.theme().metrics().padding_x * 2);
        return {static_cast<int>(w + 0.5f), ctx.theme().metrics().control_h};
    }

    void button::paint(frame_ctx& ctx) noexcept {
        const auto& pal = ctx.theme().colors();
        const bool pressed = ctx.focus().is_captured(id()) && ctx.input().key_down(VK_LBUTTON) && m_rect.contains(ctx.cursor()) &&
            !ctx.cursor_in_popup();
        const float t = m_hover_t.value();

        const math::color top = pressed ? pal.control_pressed : math::lerp(pal.control_top, pal.control_top_hover, t);
        const math::color bot = pressed ? pal.control_pressed : math::lerp(pal.control_bot, pal.control_bot_hover, t);

        ctx.pnt().filled_rect(m_rect, pal.border_outer);
        const rect inside = m_rect.inset(ctx.pnt().hairline());
        ctx.pnt().vertical_gradient(inside, top, bot);

        math::color rim = pal.border_inner;
        rim.a = 200;
        ctx.pnt().filled_rect({inside.x, inside.y, inside.w, ctx.pnt().hairline()}, rim);
        if (t > 0.0f && !pressed) {
            math::color hot = pal.text_primary;
            hot.a = static_cast<dh::u8>(60.0f * t);
            ctx.pnt().filled_rect({inside.x, inside.y, inside.w, ctx.pnt().hairline()}, hot);
        }

        if (m_confirmation && m_confirmation->is_open()) {
            const float underline = static_cast<float>(ctx.theme().metrics().active_underline_h);
            ctx.pnt().filled_rect({inside.x, m_rect.bottom() - underline, inside.w, underline}, ctx.theme().accent_value());
        }

        const dh::string_view text_view{m_text.data(), m_text.size()};
        math::color tc;
        if (pressed)
            tc = pal.text_secondary;
        else if (t > 0.0f)
            tc = pal.text_primary;
        else
            tc = pal.text_label;
        ctx.pnt().text_centered(m_rect, text_view, tc);
    }

    bool button::on_input(frame_ctx& ctx) noexcept {
        const point cursor = ctx.cursor();
        const bool inside = m_rect.contains(cursor);

        const bool blocked = ctx.cursor_in_popup();

        if (inside && !ctx.mouse_consumed() && !blocked) {
            ctx.focus().set_hovered(*this);
        }

        const float target = (inside && !blocked) ? 1.0f : 0.0f;
        m_hover_t.set_target(target, ctx.theme().times().hover);

        if (ctx.focus().is_captured(id())) {
            if (ctx.input().key_down(VK_LBUTTON)) {
                ctx.consume_mouse();
                return true;
            }

            ctx.focus().release_capture();
            const bool fire = inside && !blocked;
            ctx.consume_mouse();
            if (fire) click(ctx);
            return true;
        }

        if (ctx.input().key_pressed(VK_LBUTTON) && inside && !ctx.mouse_consumed() && !blocked) {
            ctx.consume_mouse();
            if (ctx.input().key_down(VK_LBUTTON)) {
                ctx.focus().capture(*this);
            } else if (ctx.input().key_released(VK_LBUTTON)) {
                click(ctx);
            }
            return true;
        }
        return false;
    }

    void button::click(frame_ctx& ctx) noexcept {
        if (m_confirmation) {
            m_confirmation->click(ctx);
            return;
        }
        if (m_on_click) m_on_click();
    }

}
