// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/string.h"
#include "core/dh/function.h"
#include "reactive/signal.h"
#include "ui/core/control.h"
#include "ui/input/attachment.h"
#include "text_edit.h"

namespace dh::ui::controls {

    class text_input final : public control {
    public:
        enum class echo { shown, masked };

        text_input(dh::string label, reactive::signal<dh::string>& bound, dh::size_t max_len) noexcept;
        text_input(dh::string label, reactive::signal<dh::string>& bound, dh::size_t max_len, text_domain domain) noexcept;

        text_input(dh::string label, reactive::signal<int>& bound, int min, int max) noexcept;
        ~text_input() override = default;

        text_input(const text_input&) = delete;
        text_input& operator=(const text_input&) = delete;
        text_input(text_input&&) = delete;
        text_input& operator=(text_input&&) = delete;

        void set_placeholder(dh::string p) noexcept;

        void bind_placeholder(const reactive::signal<dh::string>& source) noexcept;

        void on_submit(dh::function<void(const dh::string&)> callback) noexcept;
        void on_focus_lost() noexcept override;

        void set_commit_on_blur(bool on) noexcept;

        void set_echo(echo mode) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;
        bool on_input(frame_ctx& ctx) noexcept override;

    private:
        void apply_typed_chars(frame_ctx& ctx) noexcept;

        [[nodiscard]] const dh::string& text() const noexcept;
        void set_text(dh::string s) noexcept;

        [[nodiscard]] const dh::string& shown_text() const noexcept;

        void begin_edit() noexcept;
        void begin_edit_session() noexcept;
        void commit_edit() noexcept;
        void discard_edit() noexcept;
        void advance_edit_revision() noexcept;

        [[nodiscard]] rect field_rect(const frame_ctx& ctx) const noexcept;

        dh::string m_label;
        dh::string m_placeholder;
        const reactive::signal<dh::string>* m_placeholder_source{nullptr};
        reactive::signal<dh::string>* m_bound{nullptr};

        reactive::signal<int>* m_bound_int{nullptr};
        int m_min{0};
        int m_max{0};

        mutable dh::string m_display{};

        mutable dh::string m_masked{};
        mutable dh::u64 m_mask_source_revision{0};
        mutable bool m_mask_source_known{false};
        mutable bool m_mask_source_editing{false};
        echo m_echo{echo::shown};
        dh::size_t m_max_len;
        float m_blink_t{0.0f};

        bool m_select_all{false};
        text_domain m_domain{text_domain::ascii()};
        utf16_edit_decoder m_character_decoder;
        dh::u64 m_character_revision{0};
        dh::u64 m_typing_interval_revision{0};

        bool m_commit_on_blur{false};
        bool m_editing{false};
        bool m_edit_session{false};
        dh::function<void(const dh::string&)> m_on_submit{};
        input_attachment m_edit_lifetime{this, [](void* context) noexcept {
            auto& field = *static_cast<text_input*>(context);
            field.m_edit_session = false;
            field.m_select_all = false;
            field.m_character_decoder.reset();
        }};
        dh::string m_edit{};
        dh::u64 m_edit_revision{1};
    };

}
