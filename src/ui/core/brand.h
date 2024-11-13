// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "reactive/signal.h"

#include "core/dh/string.h"
#include "core/dh/utility.h"

namespace dh::ui {

    struct brand_halves final {
        constexpr brand_halves(dh::string_view first_tone, dh::string_view second_tone) noexcept : first(first_tone), second(second_tone) {}
        ~brand_halves() = default;

        brand_halves(const brand_halves&) noexcept = default;
        brand_halves& operator=(const brand_halves&) noexcept = default;
        brand_halves(brand_halves&&) noexcept = default;
        brand_halves& operator=(brand_halves&&) noexcept = default;

        dh::string_view first;
        dh::string_view second;
    };

    class brand final {
    public:
        static constexpr char k_tone_separator = '|';

        brand(dh::string built_in_first, dh::string built_in_second, const reactive::signal<bool>* renamed, const reactive::signal<dh::string>* custom) noexcept : m_first(dh::move(built_in_first)), m_second(dh::move(built_in_second)), m_renamed(renamed), m_custom(custom) {}
        ~brand() = default;

        brand(const brand&) = delete;
        brand& operator=(const brand&) = delete;
        brand(brand&&) = delete;
        brand& operator=(brand&&) = delete;

        [[nodiscard]] brand_halves halves() const noexcept {
            if (m_renamed == nullptr || m_custom == nullptr || !m_renamed->peek()) return built_in();
            // keep the reference, the returned views use this string's storage.
            const dh::string& custom = m_custom->peek();
            if (custom.empty()) return built_in();
            const dh::string_view whole{custom.data(), custom.size()};
            const dh::size_t cut = whole.find(k_tone_separator);
            if (cut == dh::string_view::npos) return brand_halves{whole, dh::string_view{}};
            return brand_halves{whole.substr(0, cut), whole.substr(cut + 1)};
        }

        [[nodiscard]] brand_halves built_in() const noexcept {
            return brand_halves{dh::string_view{m_first.data(), m_first.size()}, dh::string_view{m_second.data(), m_second.size()}};
        }

    private:
        static_assert(!reactive::signal<dh::string>::k_atomic, "brand needs a string reference");

        dh::string m_first;
        dh::string m_second;
        const reactive::signal<bool>* m_renamed;
        const reactive::signal<dh::string>* m_custom;
    };

}
