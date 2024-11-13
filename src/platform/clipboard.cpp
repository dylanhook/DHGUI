// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "clipboard.h"

#include "platform/strings.h"

#include "core/dh/utility.h"

#include <limits.h>
#include <Windows.h>

namespace dh::platform::clipboard {

    namespace {

        constexpr int k_open_attempts = 10;
        constexpr DWORD k_open_retry_ms = 5;
        enum class clipboard_wait { immediate, bounded };

        class clipboard_guard final {
        public:
            clipboard_guard(HWND owner, clipboard_wait wait) noexcept {
                const int attempts = wait == clipboard_wait::immediate ? 1 : k_open_attempts;
                for (int attempt = 0; attempt < attempts; ++attempt) {
                    if (::OpenClipboard(owner) != 0) {
                        m_open = true;
                        return;
                    }
                    if (attempt + 1 < attempts) ::Sleep(k_open_retry_ms);
                }
            }

            ~clipboard_guard() {
                if (m_open) ::CloseClipboard();
            }

            clipboard_guard(const clipboard_guard&) = delete;
            clipboard_guard& operator=(const clipboard_guard&) = delete;
            clipboard_guard(clipboard_guard&&) = delete;
            clipboard_guard& operator=(clipboard_guard&&) = delete;

            [[nodiscard]] bool open() const noexcept { return m_open; }

        private:
            bool m_open{false};
        };

        class global_lock final {
        public:
            explicit global_lock(HGLOBAL block) noexcept : m_block(block), m_pointer(block != nullptr ? ::GlobalLock(block) : nullptr) {}

            ~global_lock() {
                if (m_pointer != nullptr) ::GlobalUnlock(m_block);
            }

            global_lock(const global_lock&) = delete;
            global_lock& operator=(const global_lock&) = delete;
            global_lock(global_lock&&) = delete;
            global_lock& operator=(global_lock&&) = delete;

            [[nodiscard]] void* get() const noexcept { return m_pointer; }

        private:
            HGLOBAL m_block;
            void* m_pointer;
        };

    }

    dh::string_view to_string(clipboard_error e) noexcept {
        switch (e) {
            case clipboard_error::no_owner_window:
                return "no_owner_window";
            case clipboard_error::open_failed:
                return "open_failed";
            case clipboard_error::empty:
                return "empty";
            case clipboard_error::alloc_failed:
                return "alloc_failed";
            case clipboard_error::set_failed:
                return "set_failed";
            case clipboard_error::convert_failed:
                return "convert_failed";
            case clipboard_error::unterminated:
                return "unterminated";
            case clipboard_error::too_large:
                return "too_large";
        }
        return "unknown";
    }

    dh::expected<void, clipboard_error> set_text(dh::string_view text, void* owner_window) noexcept {
        HWND owner = static_cast<HWND>(owner_window);
        if (owner == nullptr) return dh::unexpected{clipboard_error::no_owner_window};

        auto converted = strings::to_utf16(text);
        if (!converted) return dh::unexpected{clipboard_error::convert_failed};
        const dh::wstring wide = dh::move(converted.value());

        const clipboard_guard guard{owner, clipboard_wait::bounded};
        if (!guard.open()) return dh::unexpected{clipboard_error::open_failed};

        const dh::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL block = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (block == nullptr) return dh::unexpected{clipboard_error::alloc_failed};

        {
            const global_lock locked{block};
            if (locked.get() == nullptr) {
                ::GlobalFree(block);
                return dh::unexpected{clipboard_error::alloc_failed};
            }
            dh::memcpy(locked.get(), wide.c_str(), bytes);
        }

        if (::EmptyClipboard() == 0) {
            ::GlobalFree(block);
            return dh::unexpected{clipboard_error::set_failed};
        }

        if (::SetClipboardData(CF_UNICODETEXT, block) == nullptr) {
            ::GlobalFree(block);
            return dh::unexpected{clipboard_error::set_failed};
        }
        return {};
    }

    namespace {
    [[nodiscard]] dh::expected<dh::string, clipboard_error> read_text(dh::size_t max_utf8_bytes, clipboard_wait wait) noexcept {
        const clipboard_guard guard{nullptr, wait};
        if (!guard.open()) return dh::unexpected{clipboard_error::open_failed};

        const HANDLE block = ::GetClipboardData(CF_UNICODETEXT);
        if (block == nullptr) return dh::unexpected{clipboard_error::empty};

        const global_lock locked{block};
        const auto* source = static_cast<const wchar_t*>(locked.get());
        if (source == nullptr) return dh::unexpected{clipboard_error::alloc_failed};

        const dh::size_t allocation = ::GlobalSize(block);
        if (allocation % sizeof(wchar_t) != 0) return dh::unexpected{clipboard_error::unterminated};
        const dh::size_t max_chars = allocation / sizeof(wchar_t);

        dh::size_t len = 0;
        while (len < max_chars && source[len] != L'\0') {
            if (len >= max_utf8_bytes) return dh::unexpected{clipboard_error::too_large};
            ++len;
        }
        if (len == max_chars) return dh::unexpected{clipboard_error::unterminated};
        if (len > static_cast<dh::size_t>(INT_MAX)) return dh::unexpected{clipboard_error::too_large};

        const int utf8_bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
        if (len != 0 && utf8_bytes <= 0) return dh::unexpected{clipboard_error::convert_failed};
        if (static_cast<dh::size_t>(utf8_bytes) > max_utf8_bytes) return dh::unexpected{clipboard_error::too_large};

        auto converted = strings::to_utf8(dh::wstring_view{source, len});
        if (!converted) return dh::unexpected{clipboard_error::convert_failed};
        return dh::move(converted.value());
    }
    }

    dh::expected<dh::string, clipboard_error> get_text(dh::size_t max_utf8_bytes) noexcept {
        return read_text(max_utf8_bytes, clipboard_wait::bounded);
    }

    dh::expected<dh::string, clipboard_error> get_text_now(dh::size_t max_utf8_bytes) noexcept {
        return read_text(max_utf8_bytes, clipboard_wait::immediate);
    }

}
