// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "render/image_texture.h"

#include "core/dh/array.h"
#include "core/dh/utility.h"
#include "core/dh/vector.h"

namespace dh::render::image {

    dh::string to_string(texture_error error) noexcept {
        switch (error) {
            case texture_error::device_unavailable: return dh::string{"device_unavailable"};
            case texture_error::invalid_pixels: return dh::string{"invalid_pixels"};
            case texture_error::texture_create_failed: return dh::string{"texture_create_failed"};
            case texture_error::view_create_failed: return dh::string{"view_create_failed"};
            case texture_error::usage_unsupported: return dh::string{"usage_unsupported"};
        }
        return dh::string{"unknown"};
    }

    namespace {

        constexpr dh::u32 k_max_levels = 12;
        static_assert((1u << (k_max_levels - 1)) == k_max_dimension, "k_max_levels must match the largest image mip chain");

        constexpr dh::size_t k_alpha_byte = 3;
        constexpr dh::size_t k_color_channels = 3;

        struct level final {
        public:
            level() noexcept = default;
            ~level() = default;
            level(const level&) = delete;
            level& operator=(const level&) = delete;
            level(level&&) noexcept = default;
            level& operator=(level&&) noexcept = default;
            dh::vector<dh::u8> rgba{};
            dh::u32 width{0};
            dh::u32 height{0};
        };

        [[nodiscard]] dh::u32 level_count(dh::u32 width, dh::u32 height) noexcept {
            dh::u32 longest = width > height ? width : height;
            dh::u32 count = 1;
            while (longest > 1) {
                longest >>= 1;
                ++count;
            }
            return count;
        }

        [[nodiscard]] constexpr dh::u32 range_begin(dh::u32 index, dh::u32 source, dh::u32 destination) noexcept {
            return static_cast<dh::u32>(static_cast<dh::u64>(index) * source / destination);
        }
        [[nodiscard]] constexpr dh::u32 range_end(dh::u32 index, dh::u32 source, dh::u32 destination) noexcept {
            const dh::u32 begin = range_begin(index, source, destination);
            const dh::u32 end = range_begin(index + 1, source, destination);
            return end > begin ? end : begin + 1;
        }

        [[nodiscard]] level downsample(const level& from) noexcept {
            level to;
            to.width = from.width > 1 ? from.width / 2 : 1;
            to.height = from.height > 1 ? from.height / 2 : 1;
            to.rgba.assign(static_cast<dh::size_t>(to.width) * to.height * k_bytes_per_pixel, dh::u8{0});
            for (dh::u32 y = 0; y < to.height; ++y) {
                const dh::u32 sy0 = range_begin(y, from.height, to.height);
                const dh::u32 sy1 = range_end(y, from.height, to.height);
                for (dh::u32 x = 0; x < to.width; ++x) {
                    const dh::u32 sx0 = range_begin(x, from.width, to.width);
                    const dh::u32 sx1 = range_end(x, from.width, to.width);
                    dh::array<dh::u64, k_color_channels> weighted{};
                    dh::u64 alpha = 0;
                    dh::u64 texels = 0;
                    for (dh::u32 sy = sy0; sy < sy1; ++sy) {
                        for (dh::u32 sx = sx0; sx < sx1; ++sx) {
                            const dh::u8* const texel = from.rgba.data() + (static_cast<dh::size_t>(sy) * from.width + sx) * k_bytes_per_pixel;
                            const dh::u64 a = texel[k_alpha_byte];
                            for (dh::size_t c = 0; c < k_color_channels; ++c) weighted[c] += texel[c] * a;
                            alpha += a;
                            ++texels;
                        }
                    }
                    dh::u8* const out = to.rgba.data() + (static_cast<dh::size_t>(y) * to.width + x) * k_bytes_per_pixel;
                    if (alpha != 0) {
                        for (dh::size_t c = 0; c < k_color_channels; ++c) out[c] = static_cast<dh::u8>((weighted[c] + alpha / 2) / alpha);
                    }
                    out[k_alpha_byte] = static_cast<dh::u8>((alpha + texels / 2) / texels);
                }
            }
            return to;
        }

        void dilate_edges(level& target) noexcept {
            // extend rgb into transparent pixels without changing alpha, this avoids dark edges.
            const dh::vector<dh::u8> source = target.rgba;
            const auto at = [&source, &target](dh::u32 x, dh::u32 y) noexcept {
                return source.data() + (static_cast<dh::size_t>(y) * target.width + x) * k_bytes_per_pixel;
            };
            for (dh::u32 y = 0; y < target.height; ++y) {
                for (dh::u32 x = 0; x < target.width; ++x) {
                    if (at(x, y)[k_alpha_byte] != 0) continue;
                    dh::array<dh::u64, k_color_channels> weighted{};
                    dh::u64 alpha = 0;
                    const dh::u32 y0 = y > 0 ? y - 1 : 0;
                    const dh::u32 y1 = y + 1 < target.height ? y + 1 : y;
                    const dh::u32 x0 = x > 0 ? x - 1 : 0;
                    const dh::u32 x1 = x + 1 < target.width ? x + 1 : x;
                    for (dh::u32 ny = y0; ny <= y1; ++ny) {
                        for (dh::u32 nx = x0; nx <= x1; ++nx) {
                            const dh::u8* const neighbor = at(nx, ny);
                            const dh::u64 a = neighbor[k_alpha_byte];
                            for (dh::size_t c = 0; c < k_color_channels; ++c) weighted[c] += neighbor[c] * a;
                            alpha += a;
                        }
                    }
                    if (alpha == 0) continue;
                    dh::u8* const out = target.rgba.data() + (static_cast<dh::size_t>(y) * target.width + x) * k_bytes_per_pixel;
                    for (dh::size_t c = 0; c < k_color_channels; ++c) out[c] = static_cast<dh::u8>((weighted[c] + alpha / 2) / alpha);
                }
            }
        }

    }

    dh::expected<image_texture, texture_failure> create_texture(ID3D11Device* device, dh::span<const dh::u8> rgba, dh::u32 width, dh::u32 height, mip_levels levels, texture_usage usage) noexcept {
        if (device == nullptr) return dh::unexpected{texture_failure{texture_error::device_unavailable}};
        if (usage == texture_usage::updatable && levels != mip_levels::single) return dh::unexpected{texture_failure{texture_error::usage_unsupported}};
        if (width == 0 || height == 0 || width > k_max_dimension || height > k_max_dimension || rgba.size() != static_cast<dh::size_t>(width) * height * k_bytes_per_pixel) {
            return dh::unexpected{texture_failure{texture_error::invalid_pixels}};
        }

        dh::vector<level> chain;
        dh::array<D3D11_SUBRESOURCE_DATA, k_max_levels> initial{};
        dh::u32 count = 1;
        if (levels == mip_levels::full_chain) {
            count = level_count(width, height);
            chain.reserve(count);
            level first;
            first.width = width;
            first.height = height;
            first.rgba.assign(rgba.begin(), rgba.end());
            dilate_edges(first);
            chain.push_back(dh::move(first));
            while (chain.size() < count) {
                level next = downsample(chain.back());
                dilate_edges(next);
                chain.push_back(dh::move(next));
            }
            for (dh::u32 i = 0; i < count; ++i) {
                initial[i].pSysMem = chain[i].rgba.data();
                initial[i].SysMemPitch = chain[i].width * static_cast<UINT>(k_bytes_per_pixel);
                initial[i].SysMemSlicePitch = static_cast<UINT>(chain[i].rgba.size());
            }
        } else {
            initial[0].pSysMem = rgba.data();
            initial[0].SysMemPitch = width * static_cast<UINT>(k_bytes_per_pixel);
            initial[0].SysMemSlicePitch = static_cast<UINT>(rgba.size());
        }

        D3D11_TEXTURE2D_DESC description{};
        description.Width = width;
        description.Height = height;
        description.MipLevels = count;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = usage == texture_usage::updatable ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        image_texture out;
        if (const HRESULT hr = device->CreateTexture2D(&description, initial.data(), out.texture.GetAddressOf()); FAILED(hr) || !out.texture) {
            return dh::unexpected{texture_failure{texture_error::texture_create_failed, hr}};
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC view_description{};
        view_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view_description.Texture2D.MostDetailedMip = 0;
        view_description.Texture2D.MipLevels = count;
        if (const HRESULT hr = device->CreateShaderResourceView(out.texture.Get(), &view_description, out.view.GetAddressOf()); FAILED(hr) || !out.view) {
            return dh::unexpected{texture_failure{texture_error::view_create_failed, hr}};
        }
        out.width = width;
        out.height = height;
        return out;
    }

}
