// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "render/image_decoder.h"

#include "core/dh/expected.h"
#include "core/dh/span.h"
#include "core/dh/string.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace dh::render::image {

    // full_chain weights rgb by alpha so cutouts dont get dark edges.
    enum class mip_levels : dh::u8 { single, full_chain };

    enum class texture_usage : dh::u8 { immutable, updatable };

    enum class texture_error : dh::u8 {
        device_unavailable,
        invalid_pixels,
        texture_create_failed,
        view_create_failed,
        usage_unsupported,
    };
    [[nodiscard]] dh::string to_string(texture_error error) noexcept;

    struct texture_failure final {
    public:
        constexpr texture_failure(texture_error cause_value, HRESULT hr_value = S_OK) noexcept : cause(cause_value), hr(hr_value) {}
        ~texture_failure() = default;
        texture_failure(const texture_failure&) = default;
        texture_failure& operator=(const texture_failure&) = default;
        texture_failure(texture_failure&&) noexcept = default;
        texture_failure& operator=(texture_failure&&) noexcept = default;
        texture_error cause;
        HRESULT hr{S_OK};
    };

    struct image_texture final {
    public:
        image_texture() noexcept = default;
        ~image_texture() = default;
        image_texture(const image_texture&) = delete;
        image_texture& operator=(const image_texture&) = delete;
        image_texture(image_texture&&) noexcept = default;
        image_texture& operator=(image_texture&&) noexcept = default;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture{};
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view{};
        dh::u32 width{0};
        dh::u32 height{0};
    };

    [[nodiscard]] dh::expected<image_texture, texture_failure> create_texture(ID3D11Device* device, dh::span<const dh::u8> rgba, dh::u32 width, dh::u32 height, mip_levels levels, texture_usage usage) noexcept;

}
