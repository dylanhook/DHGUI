// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "app.h"

#include "core/dh/chrono.h"
#include "core/dh/memory.h"
#include "core/dh/string.h"

#include <objbase.h>
#include <shellapi.h>

namespace {

    constexpr int k_exit_no_heap = 3;
    constexpr int k_exit_bad_arguments = 4;
    constexpr int k_exit_no_com = 5;
    constexpr int k_exit_no_clock = 6;

    class com_apartment final {
    public:
        com_apartment() noexcept : m_hr(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
        ~com_apartment() {
            if (SUCCEEDED(m_hr)) ::CoUninitialize();
        }
        com_apartment(const com_apartment&) = delete;
        com_apartment& operator=(const com_apartment&) = delete;
        com_apartment(com_apartment&&) = delete;
        com_apartment& operator=(com_apartment&&) = delete;

        [[nodiscard]] bool ready() const noexcept { return SUCCEEDED(m_hr); }

    private:
        HRESULT m_hr;
    };

    class argument_list final {
    public:
        argument_list() noexcept : m_args(::CommandLineToArgvW(::GetCommandLineW(), &m_count)) {}
        ~argument_list() {
            if (m_args != nullptr) ::LocalFree(m_args);
        }
        argument_list(const argument_list&) = delete;
        argument_list& operator=(const argument_list&) = delete;
        argument_list(argument_list&&) = delete;
        argument_list& operator=(argument_list&&) = delete;

        [[nodiscard]] int count() const noexcept { return m_args != nullptr ? m_count : 0; }
        [[nodiscard]] dh::wstring_view at(int index) const noexcept { return dh::wstring_view{m_args[index]}; }

    private:
        int m_count{0};
        LPWSTR* m_args;
    };

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (!dh::init_private_heap()) return k_exit_no_heap;
    if (!dh::chrono::prime_qpc_frequency()) return k_exit_no_clock;
    const com_apartment com;
    if (!com.ready()) return k_exit_no_com;

    const argument_list args;
    if (args.count() == 1) return dh::demo_menu::run_interactive();
    if (args.count() == 3 && args.at(1) == dh::wstring_view{L"--capture"}) return dh::demo_menu::run_capture(args.at(2));
    return k_exit_bad_arguments;
}
