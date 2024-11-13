// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "device.h"

#include "core/dh/array.h"
#include "core/dh/memory.h"

namespace dh::demo_menu {

    namespace {

        constexpr DXGI_FORMAT k_backbuffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;

        constexpr UINT k_buffer_count = 2;
        constexpr dh::u32 k_bytes_per_pixel = 4;

    }

    dh::string_view to_string(device_error error) noexcept {
        switch (error) {
            case device_error::create_failed: return "create_failed";
            case device_error::resize_failed: return "resize_failed";
            case device_error::present_failed: return "present_failed";
            case device_error::backbuffer_unavailable: return "backbuffer_unavailable";
            case device_error::staging_failed: return "staging_failed";
            case device_error::map_failed: return "map_failed";
        }
        return "unknown_device_error";
    }

    dh::expected<void, device_failure> device::create(HWND window, dh::u32 width, dh::u32 height) noexcept {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = k_buffer_count;
        desc.BufferDesc.Width = width;
        desc.BufferDesc.Height = height;
        desc.BufferDesc.Format = k_backbuffer_format;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        const dh::array<D3D_FEATURE_LEVEL, 1> levels{D3D_FEATURE_LEVEL_11_0};
        const HRESULT hr = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels.data(), static_cast<UINT>(levels.size()), D3D11_SDK_VERSION, &desc, &m_swapchain, &m_device, nullptr, &m_context);
        if (FAILED(hr)) return dh::unexpected{device_failure{device_error::create_failed, hr}};
        return {};
    }

    dh::expected<void, device_failure> device::resize(dh::u32 width, dh::u32 height) noexcept {
        const HRESULT hr = m_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return dh::unexpected{device_failure{device_error::resize_failed, hr}};
        return {};
    }

    dh::expected<void, device_failure> device::present() noexcept {
        const HRESULT hr = m_swapchain->Present(1, 0);
        if (FAILED(hr)) return dh::unexpected{device_failure{device_error::present_failed, hr}};
        return {};
    }

    void device::clear(const float (&rgba)[4]) noexcept {
        com_ptr<ID3D11Texture2D> backbuffer;
        if (FAILED(m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)))) return;
        com_ptr<ID3D11RenderTargetView> target;
        if (FAILED(m_device->CreateRenderTargetView(backbuffer.Get(), nullptr, &target))) return;
        m_context->ClearRenderTargetView(target.Get(), rgba);
    }

    dh::expected<frame_pixels, device_failure> device::read_back() noexcept {
        com_ptr<ID3D11Texture2D> backbuffer;
        if (const HRESULT hr = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)); FAILED(hr))
            return dh::unexpected{device_failure{device_error::backbuffer_unavailable, hr}};

        D3D11_TEXTURE2D_DESC desc{};
        backbuffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        com_ptr<ID3D11Texture2D> staging;
        if (const HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &staging); FAILED(hr))
            return dh::unexpected{device_failure{device_error::staging_failed, hr}};
        m_context->CopyResource(staging.Get(), backbuffer.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (const HRESULT hr = m_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped); FAILED(hr))
            return dh::unexpected{device_failure{device_error::map_failed, hr}};

        frame_pixels pixels;
        pixels.width = desc.Width;
        pixels.height = desc.Height;
        const dh::size_t row_bytes = static_cast<dh::size_t>(desc.Width) * k_bytes_per_pixel;
        pixels.rgba.resize(row_bytes * desc.Height);
        const auto* source = static_cast<const dh::u8*>(mapped.pData);
        for (dh::u32 row = 0; row < desc.Height; ++row) {
            dh::memcpy(pixels.rgba.data() + row * row_bytes, source + static_cast<dh::size_t>(row) * mapped.RowPitch, row_bytes);
        }
        m_context->Unmap(staging.Get(), 0);
        return pixels;
    }

}
