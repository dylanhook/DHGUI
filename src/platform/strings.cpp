// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "strings.h"

#include "core/dh/memory.h"
#include "core/dh/numeric_limits.h"
#include "core/dh/utility.h"

#include <Windows.h>

namespace dh::platform::strings {

    namespace {

        [[nodiscard]] convert_error sizing_error() noexcept {
            return ::GetLastError() == ERROR_NO_UNICODE_TRANSLATION ? convert_error::invalid_sequence : convert_error::size_query_failed;
        }

    }

    dh::expected<dh::string, convert_error> to_utf8(dh::wstring_view wide) noexcept {
        if (wide.empty()) return dh::string{};
        if (wide.size() > static_cast<dh::size_t>(dh::numeric_limits<int>::max())) {
            return dh::unexpected{convert_error::input_too_large};
        }

        const int needed = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return dh::unexpected{sizing_error()};

        dh::string out(static_cast<dh::size_t>(needed), '\0');
        const int written = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), out.data(), needed, nullptr, nullptr);
        if (written <= 0) return dh::unexpected{convert_error::conversion_failed};

        out.resize(static_cast<dh::size_t>(written));
        return dh::move(out);
    }

    dh::expected<dh::wstring, convert_error> to_utf16(dh::string_view narrow) noexcept {
        if (narrow.empty()) return dh::wstring{};
        if (narrow.size() > static_cast<dh::size_t>(dh::numeric_limits<int>::max())) {
            return dh::unexpected{convert_error::input_too_large};
        }

        const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.data(), static_cast<int>(narrow.size()), nullptr, 0);
        if (needed <= 0) return dh::unexpected{sizing_error()};

        dh::wstring out(static_cast<dh::size_t>(needed), L'\0');
        const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.data(), static_cast<int>(narrow.size()), out.data(), needed);
        if (written <= 0) return dh::unexpected{convert_error::conversion_failed};

        out.resize(static_cast<dh::size_t>(written));
        return dh::move(out);
    }

    void to_lower_ascii(dh::span<wchar_t> chars) noexcept {
        for (auto& c : chars) {
            if (c >= L'A' && c <= L'Z') {
                c = static_cast<wchar_t>(c + (L'a' - L'A'));
            }
        }
    }

    dh::expected<dh::size_t, convert_error> copy_utf16(dh::string_view narrow, dh::span<wchar_t> output) noexcept {
        if (narrow.empty()) return dh::size_t{0};
        if (output.empty()) return dh::unexpected{convert_error::output_too_small};
        if (narrow.size() > static_cast<dh::size_t>(dh::numeric_limits<int>::max()) || output.size() > static_cast<dh::size_t>(dh::numeric_limits<int>::max())) return dh::unexpected{convert_error::input_too_large};
        const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.data(), static_cast<int>(narrow.size()), output.data(), static_cast<int>(output.size()));
        if (written <= 0) return dh::unexpected{::GetLastError() == ERROR_INSUFFICIENT_BUFFER ? convert_error::output_too_small : sizing_error()};
        return static_cast<dh::size_t>(written);
    }

    dh::expected<dh::size_t, convert_error> utf8_scalar_count(dh::string_view narrow) noexcept {
        if (narrow.empty()) return dh::size_t{0};
        if (narrow.size() > static_cast<dh::size_t>(dh::numeric_limits<int>::max())) return dh::unexpected{convert_error::input_too_large};
        const int units = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrow.data(), static_cast<int>(narrow.size()), nullptr, 0);
        if (units <= 0) return dh::unexpected{sizing_error()};
        constexpr unsigned char k_continuation_mask = 0xC0;
        constexpr unsigned char k_continuation_tag = 0x80;
        dh::size_t scalars = 0;
        for (const unsigned char byte : narrow) if ((byte & k_continuation_mask) != k_continuation_tag) ++scalars;
        return scalars;
    }

}
