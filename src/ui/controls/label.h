// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "math/color.h"
#include "ui/core/control.h"

#include "core/dh/optional.h"
#include "core/dh/string.h"

namespace dh::ui::controls {

    class label final : public control {
    public:
        explicit label(dh::string text) noexcept;
        ~label() override = default;

        label(const label&) = delete;
        label& operator=(const label&) = delete;
        label(label&&) = delete;
        label& operator=(label&&) = delete;

        void set_text(dh::string text) noexcept;
        void set_color(math::color c) noexcept;
        void set_wrap(bool on) noexcept;

        [[nodiscard]] size desired_size(const frame_ctx& ctx) const noexcept override;
        void paint(frame_ctx& ctx) noexcept override;

    private:
        dh::string m_text;
        dh::optional<math::color> m_override_color;
        bool m_wrap{false};
    };

}
