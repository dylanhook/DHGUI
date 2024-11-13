// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/string.h"
#include "core/dh/vector.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace dh::demo_menu {

    enum class device_error : dh::u8 {
        create_failed,
        resize_failed,
        present_failed,
        backbuffer_unavailable,
        staging_failed,
        map_failed,
    };

    [[nodiscard]] dh::string_view to_string(device_error error) noexcept;

    struct device_failure final {
        device_failure(device_error failure_cause, HRESULT result) noexcept : cause(failure_cause), hr(result) {}
        ~device_failure() = default;
        device_failure(const device_failure&) noexcept = default;
        device_failure& operator=(const device_failure&) noexcept = default;
        device_failure(device_failure&&) noexcept = default;
        device_failure& operator=(device_failure&&) noexcept = default;

        device_error cause;
        HRESULT hr;
    };

    struct frame_pixels final {
        frame_pixels() noexcept = default;
        ~frame_pixels() = default;
        frame_pixels(const frame_pixels&) = delete;
        frame_pixels& operator=(const frame_pixels&) = delete;
        frame_pixels(frame_pixels&&) noexcept = default;
        frame_pixels& operator=(frame_pixels&&) noexcept = default;

        dh::u32 width{0};
        dh::u32 height{0};
        dh::vector<dh::u8> rgba;
    };

    class device final {
    public:
        device() noexcept = default;
        ~device() = default;

        device(const device&) = delete;
        device& operator=(const device&) = delete;
        device(device&&) = delete;
        device& operator=(device&&) = delete;

        [[nodiscard]] dh::expected<void, device_failure> create(HWND window, dh::u32 width, dh::u32 height) noexcept;
        [[nodiscard]] dh::expected<void, device_failure> resize(dh::u32 width, dh::u32 height) noexcept;
        [[nodiscard]] dh::expected<void, device_failure> present() noexcept;

        void clear(const float (&rgba)[4]) noexcept;

        [[nodiscard]] dh::expected<frame_pixels, device_failure> read_back() noexcept;

        [[nodiscard]] IDXGISwapChain* swapchain() const noexcept { return m_swapchain.Get(); }
        [[nodiscard]] ID3D11Device* native_device() const noexcept { return m_device.Get(); }

    private:
        template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>;

        com_ptr<ID3D11Device> m_device;
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<IDXGISwapChain> m_swapchain;
    };

}
