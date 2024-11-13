// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/signal.h"
#include "ui/core/types.h"

#include "core/dh/function.h"
#include "core/dh/span.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"
#include "core/dh/vector.h"

#include "core/dh/memory.h"

namespace dh::ui {

    class frame_ctx;
    class control;

    inline constexpr dh::size_t k_max_option_set_options = sizeof(dh::u64) * 8;

    class i_context_editor {
    public:
        virtual ~i_context_editor() = default;

        i_context_editor(const i_context_editor&) = delete;
        i_context_editor& operator=(const i_context_editor&) = delete;
        i_context_editor(i_context_editor&&) = delete;
        i_context_editor& operator=(i_context_editor&&) = delete;

        [[nodiscard]] virtual bool open_on_right_click(frame_ctx& ctx, rect hit_bounds) noexcept = 0;

        virtual void set_display_name(dh::string display_name) noexcept { static_cast<void>(display_name); }

        virtual void set_number_format(dh::string format) noexcept { static_cast<void>(format); }
        virtual void set_number_value_fn(dh::function<dh::string(float)> value_fn) noexcept { static_cast<void>(value_fn); }

    protected:
        i_context_editor() noexcept = default;
    };

    class i_context_editor_factory {
    public:
        virtual ~i_context_editor_factory() = default;

        i_context_editor_factory(const i_context_editor_factory&) = delete;
        i_context_editor_factory& operator=(const i_context_editor_factory&) = delete;
        i_context_editor_factory(i_context_editor_factory&&) = delete;
        i_context_editor_factory& operator=(i_context_editor_factory&&) = delete;

        [[nodiscard]] virtual dh::unique_ptr<i_context_editor> for_boolean(control& owner, dh::string display_name, reactive::signal<bool>& bound) const noexcept = 0;

        [[nodiscard]] virtual dh::unique_ptr<i_context_editor> for_number(control& owner, dh::string display_name, reactive::signal<float>& bound, float minimum, float maximum, float step) const noexcept = 0;

        [[nodiscard]] virtual dh::unique_ptr<i_context_editor> for_option_list(control& owner, dh::string display_name, reactive::signal<int>& bound, const dh::vector<dh::string>& options) const noexcept = 0;

        [[nodiscard]] virtual dh::unique_ptr<i_context_editor> for_option_set(control& owner, dh::string display_name, reactive::signal<dh::u64>& bound, const dh::vector<dh::string>& options) const noexcept = 0;

    protected:
        i_context_editor_factory() noexcept = default;
    };

    class i_option_catalog {
    public:
        virtual ~i_option_catalog() = default;

        i_option_catalog(const i_option_catalog&) = delete;
        i_option_catalog& operator=(const i_option_catalog&) = delete;
        i_option_catalog(i_option_catalog&&) = delete;
        i_option_catalog& operator=(i_option_catalog&&) = delete;

        virtual void publish_option_names(reactive::signal<dh::u64>& bound, dh::span<const dh::string> options) const noexcept = 0;

    protected:
        i_option_catalog() noexcept = default;
    };

}
