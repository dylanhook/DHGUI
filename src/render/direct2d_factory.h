// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/memory.h"
#include "core/dh/string.h"
#include "core/dh/utility.h"

#include <d2d1.h>
#include <wrl/client.h>

namespace dh::render {

    enum class direct2d_error : dh::u8 { library_unavailable, factory_entry_unavailable, factory_create_failed };

    [[nodiscard]] inline dh::string_view to_string(direct2d_error error) noexcept {
        switch (error) {
            case direct2d_error::library_unavailable: return "direct2d_library_unavailable";
            case direct2d_error::factory_entry_unavailable: return "direct2d_factory_entry_unavailable";
            case direct2d_error::factory_create_failed: return "direct2d_factory_create_failed";
        }
        return "unknown";
    }

    struct direct2d_failure final {
    public:
        direct2d_error code;
        HRESULT hr;
        constexpr direct2d_failure(direct2d_error cause, HRESULT result) noexcept : code(cause), hr(result) {}
        ~direct2d_failure() = default;
        direct2d_failure(const direct2d_failure&) = default;
        direct2d_failure& operator=(const direct2d_failure&) = default;
        direct2d_failure(direct2d_failure&&) noexcept = default;
        direct2d_failure& operator=(direct2d_failure&&) noexcept = default;
    };

    template <typename Interface> class direct2d_factory final {
    public:
        direct2d_factory() noexcept = default;
        ~direct2d_factory() { reset(); }
        direct2d_factory(const direct2d_factory&) = delete;
        direct2d_factory& operator=(const direct2d_factory&) = delete;
        direct2d_factory(direct2d_factory&& other) noexcept
            : m_library(other.m_library), m_factory(dh::move(other.m_factory)) { other.m_library = nullptr; }
        direct2d_factory& operator=(direct2d_factory&& other) noexcept {
            if (this != &other) {
                reset();
                m_library = other.m_library;
                other.m_library = nullptr;
                m_factory = dh::move(other.m_factory);
            }
            return *this;
        }

        [[nodiscard]] dh::expected<void, direct2d_failure> initialize(D2D1_FACTORY_TYPE threading) noexcept {
            direct2d_factory incoming;
            incoming.m_library = ::LoadLibraryExW(L"d2d1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!incoming.m_library)
                return dh::unexpected{direct2d_failure{direct2d_error::library_unavailable, HRESULT_FROM_WIN32(::GetLastError())}};

            const FARPROC address = ::GetProcAddress(incoming.m_library, "D2D1CreateFactory");
            if (!address)
                return dh::unexpected{direct2d_failure{direct2d_error::factory_entry_unavailable, HRESULT_FROM_WIN32(::GetLastError())}};
            using create_factory = HRESULT(WINAPI*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
            static_assert(sizeof(create_factory) == sizeof(address));
            create_factory create = nullptr;
            dh::memcpy(&create, &address, sizeof(create));
            const HRESULT hr = create(threading, __uuidof(Interface), nullptr, reinterpret_cast<void**>(incoming.m_factory.GetAddressOf()));
            if (FAILED(hr)) return dh::unexpected{direct2d_failure{direct2d_error::factory_create_failed, hr}};
            *this = dh::move(incoming);
            return {};
        }

        void reset() noexcept {
            m_factory.Reset();
            const HMODULE library = m_library;
            m_library = nullptr;
            if (library) ::FreeLibrary(library);
        }

        [[nodiscard]] Interface* get() const noexcept { return m_factory.Get(); }
        [[nodiscard]] Interface* operator->() const noexcept { return get(); }
        [[nodiscard]] explicit operator bool() const noexcept { return get() != nullptr; }

    private:
        HMODULE m_library{nullptr};
        Microsoft::WRL::ComPtr<Interface> m_factory{};
    };

}
