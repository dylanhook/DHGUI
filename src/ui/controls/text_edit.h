// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/array.h"
#include "core/dh/expected.h"
#include "core/dh/optional.h"
#include "core/dh/span.h"
#include "core/dh/string.h"
#include "core/dh/utility.h"
#include "input/input_state.h"
#include "platform/strings.h"
#include "render/font_atlas.h"

#include <Windows.h>

namespace dh::ui::controls {

    enum class text_kind : dh::u8 { ascii, credential, unsigned_integer, signed_integer };

    using scalar_filter = bool (*)(dh::u32 scalar) noexcept;

    class text_domain final {
    public:
        [[nodiscard]] static constexpr text_domain ascii() noexcept { return {text_kind::ascii, nullptr}; }
        [[nodiscard]] static constexpr text_domain unsigned_integer() noexcept { return {text_kind::unsigned_integer, nullptr}; }
        [[nodiscard]] static constexpr text_domain signed_integer() noexcept { return {text_kind::signed_integer, nullptr}; }

        [[nodiscard]] static constexpr text_domain credential(scalar_filter admits) noexcept { return {text_kind::credential, admits}; }

        ~text_domain() = default;
        constexpr text_domain(const text_domain&) noexcept = default;
        constexpr text_domain& operator=(const text_domain&) noexcept = default;
        constexpr text_domain(text_domain&&) noexcept = default;
        constexpr text_domain& operator=(text_domain&&) noexcept = default;

        [[nodiscard]] constexpr text_kind kind() const noexcept { return m_kind; }
        [[nodiscard]] constexpr scalar_filter credential_filter() const noexcept { return m_filter; }

    private:
        constexpr text_domain(text_kind kind_value, scalar_filter filter) noexcept : m_kind(kind_value), m_filter(filter) {}

        text_kind m_kind;
        scalar_filter m_filter;
    };

    enum class text_edit_error { invalid_utf16, invalid_utf8, scalar_refused, byte_limit, unit_limit, conversion_failed };

    [[nodiscard]] constexpr bool is_credential_domain(text_domain domain) noexcept {
        return domain.kind() == text_kind::credential;
    }

    [[nodiscard]] constexpr dh::string_view to_string(text_edit_error error) noexcept {
        switch (error) {
            case text_edit_error::invalid_utf16: return "invalid_utf16";
            case text_edit_error::invalid_utf8: return "invalid_utf8";
            case text_edit_error::scalar_refused: return "scalar_refused";
            case text_edit_error::byte_limit: return "byte_limit";
            case text_edit_error::unit_limit: return "unit_limit";
            case text_edit_error::conversion_failed: return "conversion_failed";
        }
        return "unknown_text_edit_error";
    }

    class text_edit_batch final {
    public:
        text_edit_batch(dh::string text, dh::optional<text_edit_error> refusal) noexcept : m_text(dh::move(text)), m_refusal(refusal) {}
        ~text_edit_batch() { ::SecureZeroMemory(m_text.data(), m_text.size()); }
        text_edit_batch(const text_edit_batch&) = delete;
        text_edit_batch& operator=(const text_edit_batch&) = delete;
        text_edit_batch(text_edit_batch&&) noexcept = default;
        text_edit_batch& operator=(text_edit_batch&& other) noexcept {
            if (this != &other) { ::SecureZeroMemory(m_text.data(), m_text.size()); m_text = dh::move(other.m_text); m_refusal = other.m_refusal; }
            return *this;
        }
        [[nodiscard]] const dh::string& text() const noexcept { return m_text; }
        [[nodiscard]] dh::string take_text() noexcept { return dh::move(m_text); }
        [[nodiscard]] dh::optional<text_edit_error> refusal() const noexcept { return m_refusal; }
    private:
        dh::string m_text;
        dh::optional<text_edit_error> m_refusal;
    };

    namespace edit_detail {

        inline constexpr dh::u32 k_high_first = 0xD800;
        inline constexpr dh::u32 k_high_last = 0xDBFF;
        inline constexpr dh::u32 k_low_first = 0xDC00;
        inline constexpr dh::u32 k_low_last = 0xDFFF;
        inline constexpr dh::u32 k_last_unit = 0xFFFF;
        inline constexpr dh::u32 k_supplementary_base = 0x10000;
        inline constexpr unsigned k_surrogate_shift = 10;
        [[nodiscard]] constexpr bool is_high(dh::u32 value) noexcept { return value >= k_high_first && value <= k_high_last; }
        [[nodiscard]] constexpr bool is_low(dh::u32 value) noexcept { return value >= k_low_first && value <= k_low_last; }

        template <dh::size_t Capacity> class utf16_scratch final {
        public:
            utf16_scratch() noexcept = default;
            ~utf16_scratch() { ::SecureZeroMemory(units.data(), sizeof(units)); }
            utf16_scratch(const utf16_scratch&) = delete;
            utf16_scratch& operator=(const utf16_scratch&) = delete;
            utf16_scratch(utf16_scratch&&) = delete;
            utf16_scratch& operator=(utf16_scratch&&) = delete;
            dh::array<wchar_t, Capacity> units{};
        };

        [[nodiscard]] constexpr bool scalar_allowed(dh::u32 scalar, text_domain domain, dh::size_t index) noexcept {
            switch (domain.kind()) {
                case text_kind::ascii: return dh::is_printable_ascii(scalar);
                case text_kind::credential: return domain.credential_filter() != nullptr && domain.credential_filter()(scalar);
                case text_kind::unsigned_integer: return scalar >= '0' && scalar <= '9';
                case text_kind::signed_integer: return (scalar >= '0' && scalar <= '9') || (index == 0 && scalar == '-');
            }
            return false;
        }
    }

    class utf16_edit_decoder final {
    public:
        utf16_edit_decoder() noexcept = default;
        ~utf16_edit_decoder() = default;
        utf16_edit_decoder(const utf16_edit_decoder&) = delete;
        utf16_edit_decoder& operator=(const utf16_edit_decoder&) = delete;
        utf16_edit_decoder(utf16_edit_decoder&&) = delete;
        utf16_edit_decoder& operator=(utf16_edit_decoder&&) = delete;
        void reset() noexcept { m_high = 0; }

        [[nodiscard]] dh::expected<text_edit_batch, text_edit_error> decode(dh::span<const dh::u32> utf16_units) noexcept {
            if (utf16_units.size() > input::input_state::k_max_typed_chars_per_frame) { reset(); return dh::unexpected{text_edit_error::unit_limit}; }
            edit_detail::utf16_scratch<input::input_state::k_max_typed_chars_per_frame + 1> scratch;
            dh::size_t written = 0;
            dh::u32 high = m_high;
            dh::optional<text_edit_error> refusal;
            for (const dh::u32 unit : utf16_units) {
                if (high != 0) {
                    if (edit_detail::is_low(unit)) {
                        scratch.units[written++] = static_cast<wchar_t>(high);
                        scratch.units[written++] = static_cast<wchar_t>(unit);
                        high = 0;
                        continue;
                    }
                    if (!refusal) refusal = text_edit_error::invalid_utf16;
                    high = 0;
                }
                if (edit_detail::is_high(unit)) high = unit;
                else {
                    if (edit_detail::is_low(unit) || unit > edit_detail::k_last_unit) {
                        if (!refusal) refusal = text_edit_error::invalid_utf16;
                        continue;
                    }
                    scratch.units[written++] = static_cast<wchar_t>(unit);
                }
            }
            auto converted = platform::strings::to_utf8({scratch.units.data(), written});
            if (!converted) { reset(); return dh::unexpected{text_edit_error::conversion_failed}; }
            m_high = high;
            return text_edit_batch{dh::move(*converted), refusal};
        }

    private:
        dh::u32 m_high{0};
    };

    [[nodiscard]] inline dh::expected<void, text_edit_error> validate_text_edit(dh::string_view value, dh::size_t max_bytes, text_domain domain) noexcept {
        if (value.size() > max_bytes || value.size() > render::font_atlas::k_max_text_bytes) return dh::unexpected{text_edit_error::byte_limit};
        edit_detail::utf16_scratch<render::font_atlas::k_max_text_bytes> scratch;
        const auto units = platform::strings::copy_utf16(value, {scratch.units.data(), scratch.units.size()});
        if (!units) return dh::unexpected{text_edit_error::invalid_utf8};
        dh::size_t scalars = 0;
        for (dh::size_t index = 0; index < *units; ++index) {
            dh::u32 scalar = scratch.units[index];
            if (edit_detail::is_high(scalar)) {
                scalar = edit_detail::k_supplementary_base + ((scalar - edit_detail::k_high_first) << edit_detail::k_surrogate_shift) +
                    (static_cast<dh::u32>(scratch.units[++index]) - edit_detail::k_low_first);
            }
            if (!edit_detail::scalar_allowed(scalar, domain, scalars)) return dh::unexpected{text_edit_error::scalar_refused};
            ++scalars;
        }
        return {};
    }

    [[nodiscard]] inline dh::expected<dh::string, text_edit_error> append_text_edit(dh::string_view before, dh::string_view addition, dh::size_t max_bytes, text_domain domain) noexcept {
        if (!is_credential_domain(domain)) {
            if (before.size() > max_bytes || before.size() > render::font_atlas::k_max_text_bytes || addition.size() > render::font_atlas::k_max_text_bytes)
                return dh::unexpected{text_edit_error::byte_limit};
            if (!platform::strings::utf8_scalar_count(before) || !platform::strings::utf8_scalar_count(addition)) return dh::unexpected{text_edit_error::invalid_utf8};
            dh::string result{before};

            for (const unsigned char byte : addition) {
                if (result.size() >= max_bytes) break;
                if (byte < 0x80 && edit_detail::scalar_allowed(byte, domain, result.size())) result.push_back(static_cast<char>(byte));
            }
            return result;
        }
        if (before.size() > max_bytes || addition.size() > max_bytes - before.size() || before.size() + addition.size() > render::font_atlas::k_max_text_bytes)
            return dh::unexpected{text_edit_error::byte_limit};
        if (!platform::strings::utf8_scalar_count(before)) return dh::unexpected{text_edit_error::invalid_utf8};

        if (const auto valid = validate_text_edit(addition, max_bytes, domain); !valid) return dh::unexpected{valid.error()};

        dh::string result;
        result.reserve(before.size() + addition.size());
        result.append(before);
        result.append(addition);
        return result;
    }

    [[nodiscard]] inline dh::expected<text_edit_batch, text_edit_error> append_typed_text_edit(dh::string_view before, dh::string_view addition, dh::size_t max_bytes, text_domain domain) noexcept {
        if (before.size() > max_bytes || before.size() > render::font_atlas::k_max_text_bytes || addition.size() > render::font_atlas::k_max_text_bytes)
            return dh::unexpected{text_edit_error::byte_limit};
        if (!platform::strings::utf8_scalar_count(before)) return dh::unexpected{text_edit_error::invalid_utf8};
        edit_detail::utf16_scratch<render::font_atlas::k_max_text_bytes> scratch;
        const auto units = platform::strings::copy_utf16(addition, {scratch.units.data(), scratch.units.size()});
        if (!units) return dh::unexpected{text_edit_error::invalid_utf8};

        const auto requested_capacity = before.size() + addition.size();
        const auto allowed_capacity = max_bytes < render::font_atlas::k_max_text_bytes ? max_bytes : render::font_atlas::k_max_text_bytes;
        dh::string result;
        result.reserve(requested_capacity < allowed_capacity ? requested_capacity : allowed_capacity);
        result.append(before);
        dh::optional<text_edit_error> refusal;
        dh::size_t byte_position = 0;
        constexpr unsigned char k_continuation_mask = 0xC0;
        constexpr unsigned char k_continuation_tag = 0x80;
        for (dh::size_t index = 0; index < *units; ++index) {
            dh::u32 scalar = scratch.units[index];
            if (edit_detail::is_high(scalar)) scalar = edit_detail::k_supplementary_base + ((scalar - edit_detail::k_high_first) << edit_detail::k_surrogate_shift) +
                (static_cast<dh::u32>(scratch.units[++index]) - edit_detail::k_low_first);
            dh::size_t end = byte_position + 1;
            while (end < addition.size() && (static_cast<unsigned char>(addition[end]) & k_continuation_mask) == k_continuation_tag) ++end;
            const auto encoded = addition.substr(byte_position, end - byte_position);
            byte_position = end;
            if (!edit_detail::scalar_allowed(scalar, domain, result.size())) {
                if (is_credential_domain(domain) && !refusal) refusal = text_edit_error::scalar_refused;
                continue;
            }
            if (encoded.size() > max_bytes - result.size() || encoded.size() > render::font_atlas::k_max_text_bytes - result.size()) {
                if (is_credential_domain(domain) && !refusal) refusal = text_edit_error::byte_limit;
                continue;
            }
            result.append(encoded);
        }
        return text_edit_batch{dh::move(result), refusal};
    }

    [[nodiscard]] inline dh::expected<void, text_edit_error> erase_last_text_scalar(dh::string& text) noexcept {
        const auto valid = platform::strings::utf8_scalar_count({text.data(), text.size()});
        if (!valid) return dh::unexpected{text_edit_error::invalid_utf8};
        if (text.empty()) return {};
        constexpr unsigned char k_continuation_mask = 0xC0;
        constexpr unsigned char k_continuation_tag = 0x80;
        // remove the whole utf8 character, deleting one byte can leave invalid text.
        dh::size_t start = text.size() - 1;
        while (start != 0 && (static_cast<unsigned char>(text[start]) & k_continuation_mask) == k_continuation_tag) --start;
        ::SecureZeroMemory(text.data() + start, text.size() - start);
        text.resize(start);
        return {};
    }

}
