// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "d3d11_renderer.h"

#include "overlay_vs.h"
#include "overlay_ps.h"
#include "overlay_text_vs.h"
#include "overlay_text_ps.h"
#include "overlay_image_ps.h"
#include "overlay_checker_vs.h"
#include "overlay_checker_ps.h"

#include "core/dh/memory.h"

#include <d3d11.h>
#include <dxgi.h>

#include "core/dh/array.h"
#include "core/dh/string.h"
#include "core/dh/algorithm.h"
#include "core/dh/math.h"
#include "core/dh/utility.h"
#include "core/dh/numeric_limits.h"
#include "math/constants.h"

#include "log/log.h"
namespace dh::render {

    dh::string_view to_string(transform_error error) noexcept {
        switch (error) {
            case transform_error::invalid_transform: return "invalid_transform";
            case transform_error::expired_scope: return "expired_scope";
            case transform_error::coordinate_overflow: return "coordinate_overflow";
        }
        return "unknown";
    }

    dh::string_view to_string(image_refusal refusal) noexcept {
        switch (refusal) {
            case image_refusal::frame_full: return "image_frame_full";
            case image_refusal::viewport_unavailable: return "viewport_unavailable";
        }
        return "unknown";
    }

    namespace {

        [[nodiscard]] dh::array<int, 2> gradient_grid(float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept {
            constexpr float k_cell_pixels = 3.0f;
            constexpr int k_minimum_cells = 8;
            constexpr int k_maximum_cells = 64;
            const bool linear = (top_left == top_right && bottom_left == bottom_right) ||
                (top_left == bottom_left && top_right == bottom_right);
            if (linear) return {1, 1};
            const auto cells = [=](float extent) noexcept {
                return dh::clamp(static_cast<int>(dh::min(extent / k_cell_pixels, static_cast<float>(k_maximum_cells))), k_minimum_cells, k_maximum_cells);
            };
            return {cells(width), cells(height)};
        }

        [[nodiscard]] dh::string_view device_removed_reason_label(HRESULT hr) noexcept {
            switch (hr) {
                case DXGI_ERROR_DEVICE_HUNG:
                    return "device hung";
                case DXGI_ERROR_DEVICE_REMOVED:
                    return "device removed";
                case DXGI_ERROR_DEVICE_RESET:
                    return "device reset";
                case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
                    return "driver internal error";
                case DXGI_ERROR_INVALID_CALL:
                    return "invalid device call";
                case E_OUTOFMEMORY:
                    return "out of memory";
                default:
                    return "<unknown>";
            }
        }

        constexpr dh::size_t k_max_vertices_per_frame = k_max_geometry_vertices;
        constexpr dh::size_t k_max_text_vertices_per_frame = 131072;
        constexpr dh::size_t k_verts_per_triangle = 3;
        constexpr dh::size_t k_verts_per_rect = 6;
        constexpr dh::size_t k_verts_per_glyph = 6;

        constexpr dh::size_t k_max_images_per_frame = 16384;
        constexpr dh::size_t k_verts_per_image = 6;
        constexpr dh::size_t k_max_image_verts_per_frame = k_max_images_per_frame * k_verts_per_image;

        constexpr dh::size_t k_max_checkers_per_frame = 1024;
        constexpr dh::size_t k_verts_per_checker = 6;
        constexpr dh::size_t k_max_checker_verts_per_frame = k_max_checkers_per_frame * k_verts_per_checker;

        class overlay_state_backup final {
        public:
            explicit overlay_state_backup(ID3D11DeviceContext* ctx) noexcept : m_ctx(ctx) {
                if (m_ctx == nullptr) return;

                m_viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
                m_ctx->RSGetViewports(&m_viewport_count, m_viewports);
                m_scissor_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
                m_ctx->RSGetScissorRects(&m_scissor_count, m_scissors);
                m_ctx->RSGetState(m_raster_state.GetAddressOf());

                ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
                m_ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, m_dsv.GetAddressOf());
                for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
                    m_rtvs[i].Attach(rtvs[i]);
                }
                m_ctx->OMGetBlendState(m_blend_state.GetAddressOf(), m_blend_factor, &m_sample_mask);

                m_ctx->IAGetVertexBuffers(0, 1, m_vb0.GetAddressOf(), &m_vb0_stride, &m_vb0_offset);
                m_ctx->IAGetInputLayout(m_input_layout.GetAddressOf());
                m_ctx->IAGetPrimitiveTopology(&m_topology);

                m_ctx->VSGetShader(m_vs.GetAddressOf(), nullptr, nullptr);
                m_ctx->PSGetShader(m_ps.GetAddressOf(), nullptr, nullptr);
                m_ctx->GSGetShader(m_gs.GetAddressOf(), nullptr, nullptr);

                m_ctx->PSGetSamplers(0, 1, m_ps_sampler0.GetAddressOf());
                m_ctx->PSGetShaderResources(0, 1, m_ps_srv0.GetAddressOf());
            }

            ~overlay_state_backup() noexcept {
                if (m_ctx == nullptr) return;

                m_ctx->RSSetViewports(m_viewport_count, m_viewports);
                m_ctx->RSSetScissorRects(m_scissor_count, m_scissors);
                m_ctx->RSSetState(m_raster_state.Get());

                ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
                for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
                    rtvs[i] = m_rtvs[i].Get();
                }
                m_ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, m_dsv.Get());
                m_ctx->OMSetBlendState(m_blend_state.Get(), m_blend_factor, m_sample_mask);

                ID3D11Buffer* const vb0 = m_vb0.Get();
                m_ctx->IASetVertexBuffers(0, 1, &vb0, &m_vb0_stride, &m_vb0_offset);
                m_ctx->IASetInputLayout(m_input_layout.Get());
                m_ctx->IASetPrimitiveTopology(m_topology);

                m_ctx->VSSetShader(m_vs.Get(), nullptr, 0);
                m_ctx->PSSetShader(m_ps.Get(), nullptr, 0);
                m_ctx->GSSetShader(m_gs.Get(), nullptr, 0);

                ID3D11SamplerState* const sam0 = m_ps_sampler0.Get();
                m_ctx->PSSetSamplers(0, 1, &sam0);
                ID3D11ShaderResourceView* const srv0 = m_ps_srv0.Get();
                m_ctx->PSSetShaderResources(0, 1, &srv0);
            }

            overlay_state_backup(const overlay_state_backup&) = delete;
            overlay_state_backup& operator=(const overlay_state_backup&) = delete;
            overlay_state_backup(overlay_state_backup&&) = delete;
            overlay_state_backup& operator=(overlay_state_backup&&) = delete;

        private:
            template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>;

            ID3D11DeviceContext* m_ctx;

            UINT m_viewport_count{0};
            D3D11_VIEWPORT m_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
            UINT m_scissor_count{0};
            D3D11_RECT m_scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
            com_ptr<ID3D11RasterizerState> m_raster_state;

            com_ptr<ID3D11RenderTargetView> m_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> m_dsv;
            com_ptr<ID3D11BlendState> m_blend_state;
            FLOAT m_blend_factor[4]{};
            UINT m_sample_mask{0};

            com_ptr<ID3D11Buffer> m_vb0;
            UINT m_vb0_stride{0};
            UINT m_vb0_offset{0};
            com_ptr<ID3D11InputLayout> m_input_layout;
            D3D11_PRIMITIVE_TOPOLOGY m_topology{D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED};

            com_ptr<ID3D11VertexShader> m_vs;
            com_ptr<ID3D11PixelShader> m_ps;
            com_ptr<ID3D11GeometryShader> m_gs;

            com_ptr<ID3D11SamplerState> m_ps_sampler0;
            com_ptr<ID3D11ShaderResourceView> m_ps_srv0;
        };

        constexpr dh::size_t k_aa_line_vertices = 18;

        [[nodiscard]] dh::array<float, 2> rounded_contour_point(float x, float y, float w, float h, float radius, int segments_per_corner, int index, float point_radius) noexcept {
            const int points_per_corner = segments_per_corner + 1;
            const int corner = index / points_per_corner;
            const int within = index % points_per_corner;
            const float quarter = math::k_pi * 0.5f;
            const float angle = math::k_pi + static_cast<float>(corner) * quarter +
                static_cast<float>(within) * quarter / static_cast<float>(segments_per_corner);
            const float center_x = (corner == 0 || corner == 3) ? x + radius : x + w - radius;
            const float center_y = (corner == 0 || corner == 1) ? y + radius : y + h - radius;
            return dh::array<float, 2>{center_x + dh::cos(angle) * point_radius, center_y + dh::sin(angle) * point_radius};
        }

    }

    dh::string_view to_string(render_error e) noexcept {
        switch (e) {
            case render_error::null_swapchain:
                return "null_swapchain";
            case render_error::get_device_failed:
                return "get_device_failed";
            case render_error::get_buffer_failed:
                return "get_buffer_failed";
            case render_error::get_desc_failed:
                return "get_desc_failed";
            case render_error::create_rtv_failed:
                return "create_rtv_failed";
            case render_error::create_buffer_failed:
                return "create_buffer_failed";
            case render_error::create_shader_failed:
                return "create_shader_failed";
            case render_error::create_input_layout_failed:
                return "create_input_layout_failed";
            case render_error::create_blend_state_failed:
                return "create_blend_state_failed";
            case render_error::create_rasterizer_failed:
                return "create_rasterizer_failed";
            case render_error::create_sampler_failed:
                return "create_sampler_failed";
            case render_error::map_buffer_failed:
                return "map_buffer_failed";
            case render_error::device_removed:
                return "device_removed";
            case render_error::render_target_absent:
                return "render_target_absent";
            case render_error::font_table_invalid:
                return "font_table_invalid";
        }
        return "unknown";
    }

    d3d11_renderer::d3d11_renderer(log::scoped_logger log, dh::span<const font_face> fonts) noexcept : m_log(dh::move(log)), m_fonts(fonts) {
        if (const auto valid = validate_font_table(m_fonts); !valid) {
            m_font_table_failure = valid.error();
            m_log.error("invalid font table {} ({} / {} fonts), rendering disabled", to_string(valid.error()), m_fonts.size(), k_max_fonts);
            m_fonts = {};
        }
        m_pending_vertices.reserve(k_max_vertices_per_frame);

        const auto reserve_stream = [this](dh::size_t stream) noexcept {
            m_pending_text_by_font[stream].reserve(k_max_text_vertices_per_frame);
            m_pending_text_pages[stream].reserve(k_max_text_vertices_per_frame / k_verts_per_glyph);
        };
        for (dh::size_t i = 0; i < m_fonts.size(); ++i) reserve_stream(i);
        reserve_stream(k_external_text_stream);
        m_external_text_pages.reserve(k_max_text_vertices_per_frame / k_verts_per_glyph);
    }

    d3d11_renderer::~d3d11_renderer() {
        shutdown();
    }

    dh::expected<void, render_error> d3d11_renderer::begin_frame(IDXGISwapChain* swapchain) noexcept {
        if (swapchain == nullptr) return dh::unexpected{render_error::null_swapchain};

        if (m_initialized && swapchain != m_initialized_swapchain) {
            m_log.info("renderer: swapchain changed ({} -> {}), restarting", static_cast<const void*>(m_initialized_swapchain), static_cast<const void*>(swapchain));
            shutdown();
        }

        if (m_font_table_failure) return dh::unexpected{render_error::font_table_invalid};
        if (m_permanent_failure) return dh::unexpected{*m_permanent_failure};

        if (!m_initialized) {
            const auto init = initialize(swapchain);
            if (!init) {
                const bool permanent = init.error() == render_error::create_shader_failed;
                if (permanent) {
                    m_log.error("renderer init failed {}, call shutdown before retrying", to_string(init.error()));
                    m_permanent_failure = init.error();
                } else {
                    ++m_init_failure_count;

                    const auto n = m_init_failure_count;
                    if ((n & (n - 1)) == 0) {
                        m_log.warn("renderer init failed {} (attempt {}), retrying next frame", to_string(init.error()), n);
                    }
                }
                return dh::unexpected{init.error()};
            }

            m_init_failure_count = 0;
            m_initialized_swapchain = swapchain;
        }

        if (!m_rtv) return dh::unexpected{render_error::render_target_absent};

        if (++m_submission_frame == 0) dh::fail_fast("renderer frame counter overflow", FAST_FAIL_INVALID_ARG);

        m_pending_vertices.clear();
        for (auto& v : m_pending_text_by_font)
            v.clear();
        m_pending_image_verts.clear();
        m_pending_image_srvs.clear();
        m_pending_checker_verts.clear();
        for (auto& pages : m_pending_text_pages)
            pages.clear();
        for (auto& atlas : m_font_atlases)
            atlas.begin_frame();
        m_external_text_pages.clear();
        m_dropped_rects_this_frame = 0;
        m_dropped_glyphs_this_frame = 0;
        m_dropped_images_this_frame = 0;
        m_dropped_checkers_this_frame = 0;
        m_alpha_multiplier = 1.0f;
        reset_segments();
        return {};
    }

    dh::expected<void, render_error> d3d11_renderer::upload_to_buffer(ID3D11Buffer* buffer, const void* src, dh::size_t bytes) noexcept {
        if (buffer == nullptr || src == nullptr || bytes == 0) {
            return {};
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(m_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            const HRESULT removed_reason = m_device ? m_device->GetDeviceRemovedReason() : S_OK;
            if (removed_reason != S_OK) {
                if (!m_map_failed_logged) {
                    m_log.error("renderer: device removed {} hr={:#x}, waiting for a new swapchain", device_removed_reason_label(removed_reason), static_cast<unsigned>(removed_reason));
                    m_map_failed_logged = true;
                }
                return dh::unexpected{render_error::device_removed};
            }
            if (!m_map_failed_logged) {
                m_log.error("renderer: Map(WRITE_DISCARD) failed, skipping this frame");
                m_map_failed_logged = true;
            }
            return dh::unexpected{render_error::map_buffer_failed};
        }
        dh::memcpy(mapped.pData, src, bytes);
        m_context->Unmap(buffer, 0);
        return {};
    }

    void d3d11_renderer::end_frame() noexcept {
        if (m_dropped_rects_this_frame > 0 && !m_overflow_warned) {
            m_log.warn("renderer: rect buffer full, dropped {} shapes ({} vertices max)", m_dropped_rects_this_frame, k_max_vertices_per_frame);
            m_overflow_warned = true;
        } else if (m_dropped_rects_this_frame == 0) {
            m_overflow_warned = false;
        }

        if (m_dropped_glyphs_this_frame > 0 && !m_text_overflow_warned) {
            m_log.warn("renderer: text buffer full, dropped {} glyphs ({} vertices max)", m_dropped_glyphs_this_frame, k_max_text_vertices_per_frame);
            m_text_overflow_warned = true;
        } else if (m_dropped_glyphs_this_frame == 0) {
            m_text_overflow_warned = false;
        }

        if (m_dropped_images_this_frame > 0 && !m_image_overflow_warned) {
            m_log.warn("renderer: image buffer full, dropped {} images ({} max)", m_dropped_images_this_frame, k_max_images_per_frame);
            m_image_overflow_warned = true;
        } else if (m_dropped_images_this_frame == 0) {
            m_image_overflow_warned = false;
        }

        if (m_dropped_checkers_this_frame > 0 && !m_checker_overflow_warned) {
            m_log.warn("renderer: checker buffer full, dropped {} fills ({} max)", m_dropped_checkers_this_frame, k_max_checkers_per_frame);
            m_checker_overflow_warned = true;
        } else if (m_dropped_checkers_this_frame == 0) {
            m_checker_overflow_warned = false;
        }

        if (!m_context || !m_rtv) return;
        if (m_backbuffer_width == 0 || m_backbuffer_height == 0) return;

        const dh::size_t rect_pending = dh::min(m_pending_vertices.size(), k_max_vertices_per_frame);
        const dh::size_t rect_count = (rect_pending / k_verts_per_triangle) * k_verts_per_triangle;

        dh::array<dh::size_t, k_text_stream_count> text_counts{};
        dh::size_t total_text_count = 0;
        for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
            const dh::size_t pending = dh::min(m_pending_text_by_font[i].size(), k_max_text_vertices_per_frame);
            text_counts[i] = (pending / k_verts_per_glyph) * k_verts_per_glyph;
            total_text_count += text_counts[i];
        }

        const dh::size_t image_count = dh::min(m_pending_image_srvs.size(), k_max_images_per_frame);
        const dh::size_t image_vert_count = image_count * k_verts_per_image;

        const dh::size_t checker_vert_count = m_pending_checker_verts.size();

        if (rect_count == 0 && total_text_count == 0 && image_count == 0 && checker_vert_count == 0) return;

        close_current_segment();
        for (auto& s : m_segments) {
            if (s.rect_end > rect_count) s.rect_end = rect_count;
            if (s.rect_begin > s.rect_end) s.rect_begin = s.rect_end;
            for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
                if (s.text_end[i] > text_counts[i]) s.text_end[i] = text_counts[i];
                if (s.text_begin[i] > s.text_end[i]) s.text_begin[i] = s.text_end[i];
            }
            if (s.image_end > image_count) s.image_end = image_count;
            if (s.image_begin > s.image_end) s.image_begin = s.image_end;
            if (s.checker_end > checker_vert_count) s.checker_end = checker_vert_count;
            if (s.checker_begin > s.checker_end) s.checker_begin = s.checker_end;
        }

        if (image_count > 1) {
            for (const auto& s : m_segments) {
                const dh::size_t n = s.image_end - s.image_begin;
                // sorting by texture changes overlap order, only do it when this segment allows it.
                if (n < 2 || !s.sort_images) continue;

                m_image_sort_indices.clear();
                for (dh::size_t i = s.image_begin; i < s.image_end; ++i) {
                    m_image_sort_indices.push_back(i);
                }
                dh::sort_by(m_image_sort_indices.begin(), m_image_sort_indices.end(), [this](dh::size_t a, dh::size_t b) noexcept {
                        return m_pending_image_srvs[a].Get() < m_pending_image_srvs[b].Get();
                    });

                m_image_sort_srvs_scratch.clear();
                m_image_sort_verts_scratch.clear();
                for (auto i : m_image_sort_indices) {
                    m_image_sort_srvs_scratch.push_back(dh::move(m_pending_image_srvs[i]));
                    const auto* const src = &m_pending_image_verts[i * k_verts_per_image];

                    for (dh::size_t j = 0; j < k_verts_per_image; ++j) {
                        m_image_sort_verts_scratch.push_back(src[j]);
                    }
                }
                for (dh::size_t k = 0; k < n; ++k) {
                    m_pending_image_srvs[s.image_begin + k] = dh::move(m_image_sort_srvs_scratch[k]);
                    auto* const dst = &m_pending_image_verts[(s.image_begin + k) * k_verts_per_image];
                    dh::copy_n(dst, &m_image_sort_verts_scratch[k * k_verts_per_image], k_verts_per_image);
                }
            }
        }

        const auto upload_failed = [this](render_error e) noexcept {
            if (e == render_error::device_removed) shutdown();
        };
        if (rect_count > 0 && m_vertex_buffer) {
            if (const auto r = upload_to_buffer(m_vertex_buffer.Get(), m_pending_vertices.data(), sizeof(vertex) * rect_count); !r) {
                upload_failed(r.error());
                return;
            }
        }
        for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
            if (text_counts[i] > 0 && m_text_vertex_buffers[i]) {
                if (const auto r = upload_to_buffer(m_text_vertex_buffers[i].Get(), m_pending_text_by_font[i].data(), sizeof(text_vertex) * text_counts[i]); !r) {
                    upload_failed(r.error());
                    return;
                }
            }
        }
        if (image_vert_count > 0 && m_image_vertex_buffer) {
            if (const auto r = upload_to_buffer(m_image_vertex_buffer.Get(), m_pending_image_verts.data(), sizeof(text_vertex) * image_vert_count); !r) {
                upload_failed(r.error());
                return;
            }
        }
        if (checker_vert_count > 0 && m_checker_vertex_buffer) {
            if (const auto r = upload_to_buffer(m_checker_vertex_buffer.Get(), m_pending_checker_verts.data(), sizeof(checker_vertex) * checker_vert_count); !r) {
                upload_failed(r.error());
                return;
            }
        }
        m_map_failed_logged = false;

        // restore the host pipeline state even if drawing returns early.
        const overlay_state_backup state_guard{m_context.Get()};

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(m_backbuffer_width);
        viewport.Height = static_cast<float>(m_backbuffer_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);

        ID3D11RenderTargetView* rtvs[] = {m_rtv.Get()};
        m_context->OMSetRenderTargets(1, rtvs, nullptr);

        constexpr float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        m_context->OMSetBlendState(m_blend_state.Get(), blend_factor, 0xFFFFFFFFu);
        m_context->RSSetState(m_rasterizer_state.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->GSSetShader(nullptr, nullptr, 0);

        D3D11_RECT full_scissor{};
        full_scissor.left = 0;
        full_scissor.top = 0;
        full_scissor.right = static_cast<LONG>(m_backbuffer_width);
        full_scissor.bottom = static_cast<LONG>(m_backbuffer_height);

        const auto clip_device_edge = [this](double authored, LONG extent) noexcept {
            const double device = authored * m_dpi_scale;
            if (!(device > 0.0)) return LONG{0};
            if (device >= extent) return extent;

            return static_cast<LONG>(dh::clamp(to_device_edge(static_cast<float>(authored)), 0.0f, static_cast<float>(extent)));
        };

        const auto apply_scissor = [&](const draw_segment& s) noexcept {
            if (!s.has_clip) {
                m_context->RSSetScissorRects(1, &full_scissor);
                return;
            }

            D3D11_RECT sc{};
            sc.left = clip_device_edge(s.clip.x, full_scissor.right);
            sc.top = clip_device_edge(s.clip.y, full_scissor.bottom);
            sc.right = clip_device_edge(static_cast<double>(s.clip.x) + s.clip.w, full_scissor.right);
            sc.bottom = clip_device_edge(static_cast<double>(s.clip.y) + s.clip.h, full_scissor.bottom);
            if (sc.right < sc.left) sc.right = sc.left;
            if (sc.bottom < sc.top) sc.bottom = sc.top;
            m_context->RSSetScissorRects(1, &sc);
        };

        const bool rect_pipeline_ready = (rect_count > 0 && m_vertex_buffer);

        dh::array<bool, k_text_stream_count> font_ready{};
        bool any_font_ready = false;
        for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
            font_ready[i] = (text_counts[i] > 0 && m_text_vertex_buffers[i] && m_text_input_layout && m_text_vertex_shader && m_text_pixel_shader && m_text_sampler && (i == k_external_text_stream || m_font_atlases[i].is_initialized()));
            if (font_ready[i]) any_font_ready = true;
        }

        const bool image_pipeline_ready =
            (image_count > 0 && m_image_vertex_buffer && m_text_input_layout && m_text_vertex_shader && m_text_pixel_shader && m_image_pixel_shader && m_text_sampler);

        const bool checker_pipeline_ready =
            (checker_vert_count > 0 && m_checker_vertex_buffer && m_checker_input_layout && m_checker_vertex_shader && m_checker_pixel_shader);

        if (rect_pipeline_ready || any_font_ready || image_pipeline_ready || checker_pipeline_ready) {
            enum class pipeline { none, shape, checker, text, image };
            pipeline bound_pipeline = pipeline::none;
            ID3D11Buffer* bound_vertices = nullptr;
            UINT bound_stride = 0;
            ID3D11ShaderResourceView* bound_resource = nullptr;
            bool resource_bound = false;

            struct pipeline_state final {
                ID3D11InputLayout* layout;
                ID3D11VertexShader* vertex;
                ID3D11PixelShader* pixel;
            };
            const auto state_of = [&](pipeline wanted) noexcept {
                switch (wanted) {
                    case pipeline::shape: return pipeline_state{m_input_layout.Get(), m_vertex_shader.Get(), m_pixel_shader.Get()};
                    case pipeline::checker:
                        return pipeline_state{m_checker_input_layout.Get(), m_checker_vertex_shader.Get(), m_checker_pixel_shader.Get()};
                    case pipeline::text: return pipeline_state{m_text_input_layout.Get(), m_text_vertex_shader.Get(), m_text_pixel_shader.Get()};
                    case pipeline::image: return pipeline_state{m_text_input_layout.Get(), m_text_vertex_shader.Get(), m_image_pixel_shader.Get()};
                    case pipeline::none: break;
                }
                return pipeline_state{nullptr, nullptr, nullptr};
            };
            const auto bind_pipeline = [&](pipeline wanted) noexcept {
                if (wanted == bound_pipeline) return;
                const pipeline_state next = state_of(wanted);
                const pipeline_state previous = state_of(bound_pipeline);
                if (next.layout != previous.layout) m_context->IASetInputLayout(next.layout);
                if (next.vertex != previous.vertex) m_context->VSSetShader(next.vertex, nullptr, 0);
                m_context->PSSetShader(next.pixel, nullptr, 0);
                bound_pipeline = wanted;
            };
            const auto bind_vertices = [&](ID3D11Buffer* buffer, UINT stride) noexcept {
                if (buffer == bound_vertices && stride == bound_stride) return;
                constexpr UINT offset = 0;
                ID3D11Buffer* buffers[] = {buffer};
                m_context->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
                bound_vertices = buffer;
                bound_stride = stride;
            };
            const auto bind_resource = [&](ID3D11ShaderResourceView* resource) noexcept {
                if (resource_bound && resource == bound_resource) return;
                ID3D11ShaderResourceView* resources[] = {resource};
                m_context->PSSetShaderResources(0, 1, resources);
                bound_resource = resource;
                resource_bound = true;
            };

            if (any_font_ready || image_pipeline_ready) {
                ID3D11SamplerState* samplers[] = {m_text_sampler.Get()};
                m_context->PSSetSamplers(0, 1, samplers);
            }

            for (const auto& s : m_segments) {
                const bool seg_has_rects = rect_pipeline_ready && s.rect_end > s.rect_begin;
                bool seg_has_any_text = false;
                for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
                    if (font_ready[i] && s.text_end[i] > s.text_begin[i]) {
                        seg_has_any_text = true;
                        break;
                    }
                }
                const bool seg_has_images = image_pipeline_ready && s.image_end > s.image_begin;
                const bool seg_has_checkers = checker_pipeline_ready && s.checker_end > s.checker_begin;
                if (!seg_has_rects && !seg_has_checkers && !seg_has_any_text && !seg_has_images) continue;

                apply_scissor(s);

                if (seg_has_rects) {
                    bind_vertices(m_vertex_buffer.Get(), sizeof(vertex));
                    bind_pipeline(pipeline::shape);

                    const UINT count = static_cast<UINT>(s.rect_end - s.rect_begin);
                    const UINT start = static_cast<UINT>(s.rect_begin);
                    m_context->Draw(count, start);
                }

                if (seg_has_checkers) {
                    bind_pipeline(pipeline::checker);
                    bind_vertices(m_checker_vertex_buffer.Get(), sizeof(checker_vertex));
                    m_context->Draw(static_cast<UINT>(s.checker_end - s.checker_begin), static_cast<UINT>(s.checker_begin));
                }

                for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
                    if (!font_ready[i]) continue;
                    if (s.text_end[i] <= s.text_begin[i]) continue;

                    bind_pipeline(pipeline::text);
                    bind_vertices(m_text_vertex_buffers[i].Get(), sizeof(text_vertex));

                    dh::size_t first = s.text_begin[i] / k_verts_per_glyph;
                    const dh::size_t end = s.text_end[i] / k_verts_per_glyph;
                    while (first < end) {
                        auto* const page = m_pending_text_pages[i][first];
                        dh::size_t next = first + 1;
                        while (next < end && m_pending_text_pages[i][next] == page)
                            ++next;
                        bind_resource(page);
                        m_context->Draw(static_cast<UINT>((next - first) * k_verts_per_glyph), static_cast<UINT>(first * k_verts_per_glyph));
                        first = next;
                    }
                }

                if (seg_has_images) {
                    bind_pipeline(pipeline::image);
                    bind_vertices(m_image_vertex_buffer.Get(), sizeof(text_vertex));
                    for (dh::size_t i = s.image_begin; i < s.image_end; ++i) {
                        auto* const srv = m_pending_image_srvs[i].Get();
                        if (srv != nullptr) {
                            bind_resource(srv);
                            m_context->Draw(static_cast<UINT>(k_verts_per_image), static_cast<UINT>(i * k_verts_per_image));
                        }
                    }
                }
            }
        }

        m_pending_vertices.clear();
        for (auto& v : m_pending_text_by_font)
            v.clear();
        m_pending_image_verts.clear();
        m_pending_image_srvs.clear();
        m_pending_checker_verts.clear();
        m_segments.clear();
        m_clip_stack.clear();
        for (auto& pages : m_pending_text_pages)
            pages.clear();
        m_external_text_pages.clear();
    }

    void d3d11_renderer::emit_filled_rect_verts(float x, float y, float w, float h, math::color c) noexcept {
        if (m_pending_vertices.size() + k_verts_per_rect > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }

        const float vw = static_cast<float>(m_backbuffer_width);
        const float vh = static_cast<float>(m_backbuffer_height);
        if (vw <= 0.0f || vh <= 0.0f) return;

        const float dx1 = to_device_edge(x);
        const float dx2 = to_device_edge(x + w);
        const float dy1 = to_device_edge(y);
        const float dy2 = to_device_edge(y + h);

        const float x1 = (dx1 / vw) * 2.0f - 1.0f;
        const float x2 = (dx2 / vw) * 2.0f - 1.0f;
        const float y1 = 1.0f - (dy1 / vh) * 2.0f;
        const float y2 = 1.0f - (dy2 / vh) * 2.0f;

        const float cr = c.r_norm();
        const float cg = c.g_norm();
        const float cb = c.b_norm();
        const float ca = c.a_norm() * m_alpha_multiplier;

        m_pending_vertices.push_back({{x1, y1, 0.0f, 1.0f}, {cr, cg, cb, ca}});
        m_pending_vertices.push_back({{x2, y1, 0.0f, 1.0f}, {cr, cg, cb, ca}});
        m_pending_vertices.push_back({{x2, y2, 0.0f, 1.0f}, {cr, cg, cb, ca}});
        m_pending_vertices.push_back({{x1, y1, 0.0f, 1.0f}, {cr, cg, cb, ca}});
        m_pending_vertices.push_back({{x2, y2, 0.0f, 1.0f}, {cr, cg, cb, ca}});
        m_pending_vertices.push_back({{x1, y2, 0.0f, 1.0f}, {cr, cg, cb, ca}});
    }

    void d3d11_renderer::set_alpha_multiplier(float m) noexcept {
        m_alpha_multiplier = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
    }

    int d3d11_renderer::viewport_width() const noexcept {
        return static_cast<int>(static_cast<float>(m_backbuffer_width) / m_dpi_scale);
    }

    int d3d11_renderer::viewport_height() const noexcept {
        return static_cast<int>(static_cast<float>(m_backbuffer_height) / m_dpi_scale);
    }

    float d3d11_renderer::authored_viewport_w() const noexcept {
        return static_cast<float>(viewport_width());
    }

    float d3d11_renderer::authored_viewport_h() const noexcept {
        return static_cast<float>(viewport_height());
    }

    float d3d11_renderer::to_device_edge(float authored) const noexcept {
        return dh::ceil(authored * m_dpi_scale - 0.5f);
    }

    float d3d11_renderer::to_device_origin(float authored) const noexcept {
        return static_cast<float>(dh::lround(authored * m_dpi_scale));
    }

    float d3d11_renderer::stroke_thickness(float authored) const noexcept {
        const float one_device_pixel = 1.0f / m_dpi_scale;
        return (authored < one_device_pixel) ? one_device_pixel : authored;
    }

    void d3d11_renderer::bake_fonts() noexcept {
        if (!m_device) return;
        for (dh::size_t i = 0; i < m_fonts.size(); ++i) {
            font_config cfg = m_fonts[i].config;
            cfg.em_size_px *= m_dpi_scale;

            if (auto r = m_font_atlases[i].initialize(m_device.Get(), cfg); !r) {
                m_font_atlases[i].shutdown();
                m_log.warn("font '{}' failed at {}px ({}, hr={:#x}), disabled", m_fonts[i].name, cfg.em_size_px, to_string(r.error().code), static_cast<unsigned>(r.error().hr));
            } else {
                m_log.info("font '{}': {}px, line height {}, ascent {}", m_fonts[i].name, cfg.em_size_px, m_font_atlases[i].line_height(), m_font_atlases[i].ascent());
            }
        }
    }

    void d3d11_renderer::set_dpi_scale(float scale) noexcept {
        if (!dh::isfinite(scale)) return;

        const float clamped = dh::clamp(scale, k_min_dpi_scale, k_max_dpi_scale);

        if (clamped == m_dpi_scale) return;
        if (clamped != scale) {
            m_log.warn("dpi scale {} outside {} to {}, using {}", scale, k_min_dpi_scale, k_max_dpi_scale, clamped);
        }

        bool has_text = false;
        for (const auto& vertices : m_pending_text_by_font)
            has_text = has_text || !vertices.empty();
        if (!m_pending_vertices.empty() || !m_pending_image_verts.empty() || !m_pending_checker_verts.empty() || has_text) {
            m_log.warn("set dpi scale after begin_frame, before adding any draws");
            return;
        }
        m_dpi_scale = clamped;

        if (m_device) bake_fonts();
    }

    void d3d11_renderer::push_clip_rect(float x, float y, float w, float h) noexcept {
        clip_rect entry{x, y, w, h};

        if (!m_clip_stack.empty()) {
            const auto& current = m_clip_stack.back();
            const double left = (current.x > x) ? current.x : x;
            const double top = (current.y > y) ? current.y : y;
            const double current_right = static_cast<double>(current.x) + current.w;
            const double current_bottom = static_cast<double>(current.y) + current.h;
            const double right = static_cast<double>(x) + w;
            const double bottom = static_cast<double>(y) + h;

            const double width = ((current_right < right) ? current_right : right) - left;
            const double height = ((current_bottom < bottom) ? current_bottom : bottom) - top;
            entry.x = static_cast<float>(left);
            entry.y = static_cast<float>(top);
            entry.w = (width > 0.0) ? static_cast<float>((width < w) ? width : w) : 0.0f;
            entry.h = (height > 0.0) ? static_cast<float>((height < h) ? height : h) : 0.0f;
        }

        m_clip_stack.push_back(entry);

        close_current_segment();
        open_new_segment();
    }

    void d3d11_renderer::pop_clip_rect() noexcept {
        if (m_clip_stack.empty()) return;
        m_clip_stack.pop_back();
        close_current_segment();
        open_new_segment();
    }

    void d3d11_renderer::force_new_segment() noexcept {
        close_current_segment();
        open_new_segment();
    }

    void d3d11_renderer::close_current_segment() noexcept {
        if (m_segments.empty()) return;
        auto& s = m_segments.back();
        s.rect_end = m_pending_vertices.size();
        s.image_end = m_pending_image_srvs.size();
        s.checker_end = m_pending_checker_verts.size();
        for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
            s.text_end[i] = m_pending_text_by_font[i].size();
        }
    }

    void d3d11_renderer::open_new_segment() noexcept {
        draw_segment s{};
        s.rect_begin = m_pending_vertices.size();
        s.rect_end = s.rect_begin;
        s.image_begin = m_pending_image_srvs.size();
        s.image_end = s.image_begin;
        s.checker_begin = m_pending_checker_verts.size();
        s.checker_end = s.checker_begin;
        for (dh::size_t i = 0; i < k_text_stream_count; ++i) {
            s.text_begin[i] = m_pending_text_by_font[i].size();
            s.text_end[i] = s.text_begin[i];
        }
        if (!m_clip_stack.empty()) {
            s.has_clip = true;
            s.clip = m_clip_stack.back();
        }
        if (!m_segments.empty()) {
            const auto& previous = m_segments.back();
            bool empty = previous.rect_begin == s.rect_begin && previous.image_begin == s.image_begin &&
                previous.checker_begin == s.checker_begin;
            for (dh::size_t i = 0; i < k_text_stream_count; ++i)
                empty = empty && previous.text_begin[i] == s.text_begin[i];

            if (empty) {
                m_segments.back() = s;
                return;
            }
        }
        m_segments.push_back(s);
    }

    void d3d11_renderer::reset_segments() noexcept {
        m_segments.clear();
        m_clip_stack.clear();
        open_new_segment();
    }

    void d3d11_renderer::add_filled_rect(float x, float y, float w, float h, math::color c) noexcept {
        if (w <= 0.0f || h <= 0.0f) return;
        emit_filled_rect_verts(x, y, w, h, c);
    }

    void d3d11_renderer::add_checker_rect(float x, float y, float w, float h, float cell, math::color c) noexcept {
        if (!(w > 0.0f && h > 0.0f && cell > 0.0f)) return;
        if (m_pending_checker_verts.size() + k_verts_per_checker > k_max_checker_verts_per_frame) {
            ++m_dropped_checkers_this_frame;
            return;
        }
        const float vw = static_cast<float>(m_backbuffer_width);
        const float vh = static_cast<float>(m_backbuffer_height);
        if (vw <= 0.0f || vh <= 0.0f) return;

        const float dx1 = to_device_edge(x);
        const float dx2 = to_device_edge(x + w);
        const float dy1 = to_device_edge(y);
        const float dy2 = to_device_edge(y + h);
        if (dx2 <= dx1 || dy2 <= dy1) return;

        const float x1 = (dx1 / vw) * 2.0f - 1.0f;
        const float x2 = (dx2 / vw) * 2.0f - 1.0f;
        const float y1 = 1.0f - (dy1 / vh) * 2.0f;
        const float y2 = 1.0f - (dy2 / vh) * 2.0f;

        const dh::array<float, 4> grid{x, y, cell, m_dpi_scale};
        const dh::array<float, 4> color{c.r_norm(), c.g_norm(), c.b_norm(), c.a_norm() * m_alpha_multiplier};
        if (m_segments.empty()) open_new_segment();
        m_pending_checker_verts.push_back({{x1, y1, 0.0f, 1.0f}, grid, color});
        m_pending_checker_verts.push_back({{x2, y1, 0.0f, 1.0f}, grid, color});
        m_pending_checker_verts.push_back({{x2, y2, 0.0f, 1.0f}, grid, color});
        m_pending_checker_verts.push_back({{x1, y1, 0.0f, 1.0f}, grid, color});
        m_pending_checker_verts.push_back({{x2, y2, 0.0f, 1.0f}, grid, color});
        m_pending_checker_verts.push_back({{x1, y2, 0.0f, 1.0f}, grid, color});
    }

    void d3d11_renderer::add_filled_rounded_rect(float x, float y, float w, float h, float radius, math::color c, int segments_per_corner) noexcept {
        if (w <= 0.0f || h <= 0.0f || radius <= 0.0f || segments_per_corner < 1) {
            add_filled_rect(x, y, w, h, c);
            return;
        }
        radius = dh::min(radius, dh::min(w, h) * 0.5f);

        const int points_per_corner = segments_per_corner + 1;
        const int point_count = points_per_corner * 4;
        constexpr dh::size_t k_vertices_per_edge = 9;
        const dh::size_t required = static_cast<dh::size_t>(point_count) * k_vertices_per_edge;
        if (m_pending_vertices.size() + required > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }
        const float viewport_width = authored_viewport_w();
        const float viewport_height = authored_viewport_h();
        if (viewport_width <= 0.0f || viewport_height <= 0.0f) return;

        const auto to_ndc = [viewport_width, viewport_height](float px, float py) noexcept {
            return dh::array<float, 4>{
                (px / viewport_width) * 2.0f - 1.0f,
                1.0f - (py / viewport_height) * 2.0f,
                0.0f,
                1.0f,
            };
        };
        const auto point = [=](int index, float point_radius) noexcept {
            return rounded_contour_point(x, y, w, h, radius, segments_per_corner, index, point_radius);
        };

        const float alpha = c.a_norm() * m_alpha_multiplier;
        const dh::array<float, 4> opaque{c.r_norm(), c.g_norm(), c.b_norm(), alpha};
        const dh::array<float, 4> clear{c.r_norm(), c.g_norm(), c.b_norm(), 0.0f};
        constexpr float k_feather_pixels = 1.0f;
        const auto center = to_ndc(x + w * 0.5f, y + h * 0.5f);

        const int last = point_count - 1;
        auto previous_inner_point = point(last, radius);
        auto previous_outer_point = point(last, radius + k_feather_pixels);
        auto previous_inner = to_ndc(previous_inner_point[0], previous_inner_point[1]);
        auto previous_outer = to_ndc(previous_outer_point[0], previous_outer_point[1]);
        for (int index = 0; index < point_count; ++index) {
            const auto current_inner_point = point(index, radius);
            const auto current_outer_point = point(index, radius + k_feather_pixels);
            const auto current_inner = to_ndc(current_inner_point[0], current_inner_point[1]);
            const auto current_outer = to_ndc(current_outer_point[0], current_outer_point[1]);

            m_pending_vertices.push_back({center, opaque});
            m_pending_vertices.push_back({previous_inner, opaque});
            m_pending_vertices.push_back({current_inner, opaque});
            m_pending_vertices.push_back({previous_inner, opaque});
            m_pending_vertices.push_back({current_inner, opaque});
            m_pending_vertices.push_back({current_outer, clear});
            m_pending_vertices.push_back({previous_inner, opaque});
            m_pending_vertices.push_back({current_outer, clear});
            m_pending_vertices.push_back({previous_outer, clear});

            previous_inner = current_inner;
            previous_outer = current_outer;
        }
    }

    void d3d11_renderer::add_filled_circle(float cx, float cy, float radius, math::color c, int segments) noexcept {
        if (radius <= 0.0f || segments < 3) return;

        constexpr dh::size_t k_circle_verts_per_segment = 3 + 6;
        const dh::size_t need = static_cast<dh::size_t>(segments) * k_circle_verts_per_segment;
        if (m_pending_vertices.size() + need > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }
        const float vw = authored_viewport_w();
        const float vh = authored_viewport_h();
        if (vw <= 0.0f || vh <= 0.0f) return;

        const auto to_ndc = [vw, vh](float x, float y) { return dh::array<float, 4>{(x / vw) * 2.0f - 1.0f, 1.0f - (y / vh) * 2.0f, 0.0f, 1.0f}; };
        const float alpha = c.a_norm() * m_alpha_multiplier;
        const dh::array<float, 4> col_opaque{c.r_norm(), c.g_norm(), c.b_norm(), alpha};
        const dh::array<float, 4> col_clear{c.r_norm(), c.g_norm(), c.b_norm(), 0.0f};

        constexpr float k_feather_px = 1.0f;
        const float ro = radius + k_feather_px;

        const float step = math::k_two_pi / static_cast<float>(segments);
        const auto center = to_ndc(cx, cy);

        float prev_c = 1.0f;
        float prev_s = 0.0f;
        for (int i = 1; i <= segments; ++i) {
            const float angle = step * static_cast<float>(i);
            const float cur_c = dh::cos(angle);
            const float cur_s = dh::sin(angle);

            const auto rim_prev = to_ndc(cx + radius * prev_c, cy + radius * prev_s);
            const auto rim_cur = to_ndc(cx + radius * cur_c, cy + radius * cur_s);

            m_pending_vertices.push_back({center, col_opaque});
            m_pending_vertices.push_back({rim_prev, col_opaque});
            m_pending_vertices.push_back({rim_cur, col_opaque});

            const auto out_prev = to_ndc(cx + ro * prev_c, cy + ro * prev_s);
            const auto out_cur = to_ndc(cx + ro * cur_c, cy + ro * cur_s);
            m_pending_vertices.push_back({rim_prev, col_opaque});
            m_pending_vertices.push_back({rim_cur, col_opaque});
            m_pending_vertices.push_back({out_cur, col_clear});
            m_pending_vertices.push_back({rim_prev, col_opaque});
            m_pending_vertices.push_back({out_cur, col_clear});
            m_pending_vertices.push_back({out_prev, col_clear});

            prev_c = cur_c;
            prev_s = cur_s;
        }
    }

    void d3d11_renderer::add_filled_triangle_aa(float x1, float y1, float x2, float y2, float x3, float y3, const dh::array<math::color, 3>& colors) noexcept {
        constexpr dh::size_t k_aa_triangle_verts = 21;
        if (m_pending_vertices.size() + k_aa_triangle_verts > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }
        const float vw = authored_viewport_w();
        const float vh = authored_viewport_h();
        if (vw <= 0.0f || vh <= 0.0f) return;

        const auto to_ndc = [vw, vh](float px, float py) { return dh::array<float, 4>{(px / vw) * 2.0f - 1.0f, 1.0f - (py / vh) * 2.0f, 0.0f, 1.0f}; };

        dh::array<dh::array<float, 4>, 3> col_opaque{};
        dh::array<dh::array<float, 4>, 3> col_clear{};
        for (dh::size_t i = 0; i < 3; ++i) {
            const math::color& c = colors[i];
            col_opaque[i] = {c.r_norm(), c.g_norm(), c.b_norm(), c.a_norm() * m_alpha_multiplier};
            col_clear[i] = {c.r_norm(), c.g_norm(), c.b_norm(), 0.0f};
        }

        constexpr float k_feather_px = 0.5f;

        const dh::array<dh::array<float, 2>, 3> pts{{{x1, y1}, {x2, y2}, {x3, y3}}};
        const float ctr_x = (x1 + x2 + x3) / 3.0f;
        const float ctr_y = (y1 + y2 + y3) / 3.0f;

        dh::array<dh::array<float, 4>, 3> inner{};
        dh::array<dh::array<float, 4>, 3> outer{};
        for (dh::size_t i = 0; i < 3; ++i) {
            float dx = pts[i][0] - ctr_x;
            float dy = pts[i][1] - ctr_y;
            const float len = dh::sqrt(dx * dx + dy * dy);
            if (len > 1e-4f) {
                dx /= len;
                dy /= len;
            }
            inner[i] = to_ndc(pts[i][0] - dx * k_feather_px, pts[i][1] - dy * k_feather_px);
            outer[i] = to_ndc(pts[i][0] + dx * k_feather_px, pts[i][1] + dy * k_feather_px);
        }

        m_pending_vertices.push_back({inner[0], col_opaque[0]});
        m_pending_vertices.push_back({inner[1], col_opaque[1]});
        m_pending_vertices.push_back({inner[2], col_opaque[2]});

        constexpr int edge[3][2] = {{0, 1}, {1, 2}, {2, 0}};
        for (const auto& e : edge) {
            const dh::size_t s = static_cast<dh::size_t>(e[0]);
            const dh::size_t t = static_cast<dh::size_t>(e[1]);
            m_pending_vertices.push_back({inner[s], col_opaque[s]});
            m_pending_vertices.push_back({inner[t], col_opaque[t]});
            m_pending_vertices.push_back({outer[t], col_clear[t]});
            m_pending_vertices.push_back({inner[s], col_opaque[s]});
            m_pending_vertices.push_back({outer[t], col_clear[t]});
            m_pending_vertices.push_back({outer[s], col_clear[s]});
        }
    }

    void d3d11_renderer::add_gradient_triangle(float x1, float y1, float x2, float y2, float x3, float y3, const dh::array<math::color, 3>& colors) noexcept {
        constexpr dh::size_t k_triangle_verts = 3;
        if (m_pending_vertices.size() + k_triangle_verts > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }
        if (!dh::isfinite(x1) || !dh::isfinite(y1) || !dh::isfinite(x2) || !dh::isfinite(y2) || !dh::isfinite(x3) || !dh::isfinite(y3)) return;
        const float vw = authored_viewport_w();
        const float vh = authored_viewport_h();
        if (vw <= 0.0f || vh <= 0.0f) return;

        const auto to_ndc = [vw, vh](float px, float py) { return dh::array<float, 4>{(px / vw) * 2.0f - 1.0f, 1.0f - (py / vh) * 2.0f, 0.0f, 1.0f}; };
        const auto vertex_color = [this](const math::color& c) noexcept {
            return dh::array<float, 4>{c.r_norm(), c.g_norm(), c.b_norm(), c.a_norm() * m_alpha_multiplier};
        };
        m_pending_vertices.push_back({to_ndc(x1, y1), vertex_color(colors[0])});
        m_pending_vertices.push_back({to_ndc(x2, y2), vertex_color(colors[1])});
        m_pending_vertices.push_back({to_ndc(x3, y3), vertex_color(colors[2])});
    }

    void d3d11_renderer::add_gradient_rect_v(float x, float y, float w, float h, math::color top_c, math::color bottom_c) noexcept {
        add_gradient_rect(x, y, w, h, top_c, top_c, bottom_c, bottom_c);
    }

    void d3d11_renderer::add_gradient_rect(float x, float y, float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept {
        const auto result = submit_gradient(x, y, width, height, top_left, top_right, bottom_left, bottom_right);
        if (result) return;
        if (result.error().code == geometry_error::vertex_limit) ++m_dropped_rects_this_frame;
        else m_log.warn("gradient failed ({})", to_string(result.error().code));
    }

    dh::expected<dh::size_t, geometry_failure> d3d11_renderer::gradient_vertices(float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept {
        if (!dh::isfinite(width) || !dh::isfinite(height) || width < 0.0f || height < 0.0f)
            return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        if (width == 0.0f || height == 0.0f) return dh::size_t{0};
        const auto grid = gradient_grid(width, height, top_left, top_right, bottom_left, bottom_right);
        return static_cast<dh::size_t>(grid[0]) * static_cast<dh::size_t>(grid[1]) * k_verts_per_rect;
    }

    dh::expected<dh::size_t, geometry_failure> d3d11_renderer::submit_gradient(float x, float y, float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept {
        if (!m_initialized || !m_backbuffer_width || !m_backbuffer_height) return dh::unexpected{geometry_failure{geometry_error::not_initialized}};
        if (!dh::isfinite(x) || !dh::isfinite(y) || !dh::isfinite(width) || !dh::isfinite(height) || width < 0.0f || height < 0.0f || !dh::isfinite((x + width) * m_dpi_scale) || !dh::isfinite((y + height) * m_dpi_scale) || !dh::isfinite(x * m_dpi_scale) || !dh::isfinite(y * m_dpi_scale))
            return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        if (width == 0.0f || height == 0.0f) return dh::size_t{0};

        const auto grid = gradient_grid(width, height, top_left, top_right, bottom_left, bottom_right);
        const int columns = grid[0], rows = grid[1];
        const dh::size_t count = static_cast<dh::size_t>(columns) * static_cast<dh::size_t>(rows) * k_verts_per_rect;
        if (count > k_max_vertices_per_frame - m_pending_vertices.size())
            return dh::unexpected{geometry_failure{geometry_error::vertex_limit, E_BOUNDS}};
        const auto channels = [](math::color value) noexcept {
            return dh::array<float, 4>{value.r_norm(), value.g_norm(), value.b_norm(), value.a_norm()};
        };
        const auto tl = channels(top_left), tr = channels(top_right), bl = channels(bottom_left), br = channels(bottom_right);
        constexpr double k_coordinate_bound = dh::numeric_limits<float>::max();
        const auto coordinate_fits = [&](float value, dh::u32 extent) noexcept {
            const double mapped = static_cast<double>(to_device_edge(value)) / extent * 2.0;
            return mapped >= -k_coordinate_bound + 1.0 && mapped <= k_coordinate_bound - 1.0;
        };
        if (!coordinate_fits(x, m_backbuffer_width) || !coordinate_fits(x + width, m_backbuffer_width) || !coordinate_fits(y, m_backbuffer_height) || !coordinate_fits(y + height, m_backbuffer_height))
            return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        const auto corner = [&](float u, float v) noexcept {
            vertex result{};
            result.pos = {static_cast<float>(static_cast<double>(to_device_edge(x + width * u)) / m_backbuffer_width * 2.0 - 1.0),
                static_cast<float>(1.0 - static_cast<double>(to_device_edge(y + height * v)) / m_backbuffer_height * 2.0), 0.0f, 1.0f};
            for (dh::size_t index = 0; index < result.color.size(); ++index) {
                const float left = tl[index] + (bl[index] - tl[index]) * v;
                const float right = tr[index] + (br[index] - tr[index]) * v;
                result.color[index] = left + (right - left) * u;
            }
            result.color[3] *= m_alpha_multiplier;
            return result;
        };
        for (int row = 0; row < rows; ++row) {
            const float v0 = static_cast<float>(row) / static_cast<float>(rows);
            const float v1 = static_cast<float>(row + 1) / static_cast<float>(rows);
            for (int column = 0; column < columns; ++column) {
                const float u0 = static_cast<float>(column) / static_cast<float>(columns);
                const float u1 = static_cast<float>(column + 1) / static_cast<float>(columns);
                const auto a = corner(u0, v0), b = corner(u1, v0), c = corner(u1, v1), d = corner(u0, v1);
                m_pending_vertices.push_back(a); m_pending_vertices.push_back(b); m_pending_vertices.push_back(c);
                m_pending_vertices.push_back(a); m_pending_vertices.push_back(c); m_pending_vertices.push_back(d);
            }
        }
        return count;
    }

    dh::expected<dh::size_t, geometry_failure> d3d11_renderer::submit_geometry(dh::span<const D2D1_TRIANGLE> triangles, float x, float y, math::color color) noexcept {
        if (!m_initialized || !m_backbuffer_width || !m_backbuffer_height)
            return dh::unexpected{geometry_failure{geometry_error::not_initialized}};
        if (!dh::isfinite(x) || !dh::isfinite(y))
            return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        constexpr dh::size_t k_triangle_vertices = 3;
        if (triangles.size() > (k_max_vertices_per_frame - m_pending_vertices.size()) / k_triangle_vertices)
            return dh::unexpected{geometry_failure{geometry_error::vertex_limit, E_BOUNDS}};
        const double width = authored_viewport_w(), height = authored_viewport_h();
        const auto position = [&](D2D1_POINT_2F point) noexcept {
            return dh::array<double, 2>{(static_cast<double>(point.x) + x) / width * 2.0 - 1.0,
                1.0 - (static_cast<double>(point.y) + y) / height * 2.0};
        };

        constexpr double k_float_bound = dh::numeric_limits<float>::max();
        for (const auto& triangle : triangles) {
            for (const auto point : {triangle.point1, triangle.point2, triangle.point3}) {
                const auto mapped = position(point);
                if (!(mapped[0] >= -k_float_bound && mapped[0] <= k_float_bound && mapped[1] >= -k_float_bound && mapped[1] <= k_float_bound))
                    return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
            }
        }
        const dh::array<float, 4> channels{color.r_norm(), color.g_norm(), color.b_norm(), color.a_norm() * m_alpha_multiplier};
        for (const auto& triangle : triangles) {
            for (const auto point : {triangle.point1, triangle.point2, triangle.point3}) {
                const auto mapped = position(point);
                m_pending_vertices.push_back({{static_cast<float>(mapped[0]), static_cast<float>(mapped[1]), 0.0f, 1.0f}, channels});
            }
        }
        return triangles.size() * k_triangle_vertices;
    }

    void d3d11_renderer::add_outlined_rect(float x, float y, float w, float h, math::color c, float thickness) noexcept {
        if (w <= 0.0f || h <= 0.0f) return;
        if (thickness <= 0.0f) thickness = 1.0f;
        thickness = stroke_thickness(thickness);

        const float max_thickness = 0.5f * dh::min(w, h);
        if (max_thickness <= 1.0f) {
            emit_filled_rect_verts(x, y, w, h, c);
            return;
        }
        if (thickness > max_thickness) thickness = max_thickness;

        constexpr dh::size_t k_outline_verts = 4 * k_verts_per_rect;
        if (m_pending_vertices.size() + k_outline_verts > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }

        emit_filled_rect_verts(x, y, w, thickness, c);
        emit_filled_rect_verts(x, y + h - thickness, w, thickness, c);
        emit_filled_rect_verts(x, y + thickness, thickness, h - 2.0f * thickness, c);
        emit_filled_rect_verts(x + w - thickness, y + thickness, thickness, h - 2.0f * thickness, c);
    }

    void d3d11_renderer::add_line(float x1, float y1, float x2, float y2, math::color c, float thickness) noexcept {
        if (thickness <= 0.0f) thickness = 1.0f;
        thickness = stroke_thickness(thickness);

        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float len_sq = dx * dx + dy * dy;
        if (len_sq < 1e-6f) {
            emit_filled_rect_verts(x1 - thickness * 0.5f, y1 - thickness * 0.5f, thickness, thickness, c);
            return;
        }

        if (m_pending_vertices.size() + k_verts_per_rect > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }

        const float inv_len = 1.0f / dh::sqrt(len_sq);
        const float ht = thickness * 0.5f;
        const float px = -dy * inv_len * ht;
        const float py = dx * inv_len * ht;

        const float vw = authored_viewport_w();
        const float vh = authored_viewport_h();
        if (vw <= 0.0f || vh <= 0.0f) return;

        const auto to_ndc = [vw, vh](float x, float y) {
            return dh::array<float, 4>{
                (x / vw) * 2.0f - 1.0f,
                1.0f - (y / vh) * 2.0f,
                0.0f,
                1.0f,
            };
        };

        const auto corner_a = to_ndc(x1 + px, y1 + py);
        const auto corner_b = to_ndc(x2 + px, y2 + py);
        const auto corner_c = to_ndc(x2 - px, y2 - py);
        const auto corner_d = to_ndc(x1 - px, y1 - py);

        const float cr = c.r_norm();
        const float cg = c.g_norm();
        const float cb = c.b_norm();
        const float ca = c.a_norm() * m_alpha_multiplier;
        const dh::array<float, 4> col{cr, cg, cb, ca};

        m_pending_vertices.push_back({corner_a, col});
        m_pending_vertices.push_back({corner_b, col});
        m_pending_vertices.push_back({corner_c, col});
        m_pending_vertices.push_back({corner_a, col});
        m_pending_vertices.push_back({corner_c, col});
        m_pending_vertices.push_back({corner_d, col});
    }

    void d3d11_renderer::add_line_aa(float x1, float y1, float x2, float y2, math::color c, float thickness) noexcept {
        if (thickness <= 0.0f) thickness = 1.0f;
        thickness = stroke_thickness(thickness);
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float len_sq = dx * dx + dy * dy;
        if (len_sq < 1e-6f) return;

        if (m_pending_vertices.size() + k_aa_line_vertices > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }
        const float vw = authored_viewport_w();
        const float vh = authored_viewport_h();
        if (vw <= 0.0f || vh <= 0.0f) return;

        const auto to_ndc = [vw, vh](float x, float y) { return dh::array<float, 4>{(x / vw) * 2.0f - 1.0f, 1.0f - (y / vh) * 2.0f, 0.0f, 1.0f}; };
        const float alpha = c.a_norm() * m_alpha_multiplier;
        const dh::array<float, 4> col_opaque{c.r_norm(), c.g_norm(), c.b_norm(), alpha};
        const dh::array<float, 4> col_clear{c.r_norm(), c.g_norm(), c.b_norm(), 0.0f};

        const float inv_len = 1.0f / dh::sqrt(len_sq);
        const float nx = -dy * inv_len;
        const float ny = dx * inv_len;
        const float ht = thickness * 0.5f;
        constexpr float k_feather_px = 0.5f;
        const float ho = ht + k_feather_px;

        const auto a_in = to_ndc(x1 + nx * ht, y1 + ny * ht);
        const auto b_in = to_ndc(x2 + nx * ht, y2 + ny * ht);
        const auto c_in = to_ndc(x2 - nx * ht, y2 - ny * ht);
        const auto d_in = to_ndc(x1 - nx * ht, y1 - ny * ht);
        m_pending_vertices.push_back({a_in, col_opaque});
        m_pending_vertices.push_back({b_in, col_opaque});
        m_pending_vertices.push_back({c_in, col_opaque});
        m_pending_vertices.push_back({a_in, col_opaque});
        m_pending_vertices.push_back({c_in, col_opaque});
        m_pending_vertices.push_back({d_in, col_opaque});

        const auto a_out = to_ndc(x1 + nx * ho, y1 + ny * ho);
        const auto b_out = to_ndc(x2 + nx * ho, y2 + ny * ho);
        m_pending_vertices.push_back({a_in, col_opaque});
        m_pending_vertices.push_back({b_in, col_opaque});
        m_pending_vertices.push_back({b_out, col_clear});
        m_pending_vertices.push_back({a_in, col_opaque});
        m_pending_vertices.push_back({b_out, col_clear});
        m_pending_vertices.push_back({a_out, col_clear});

        const auto d_out = to_ndc(x1 - nx * ho, y1 - ny * ho);
        const auto c_out = to_ndc(x2 - nx * ho, y2 - ny * ho);
        m_pending_vertices.push_back({d_in, col_opaque});
        m_pending_vertices.push_back({c_in, col_opaque});
        m_pending_vertices.push_back({c_out, col_clear});
        m_pending_vertices.push_back({d_in, col_opaque});
        m_pending_vertices.push_back({c_out, col_clear});
        m_pending_vertices.push_back({d_out, col_clear});
    }

    dh::expected<void, image_refusal> d3d11_renderer::add_image_quad(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, float u0, float v0, float u1, float v1, math::color tint, image_order order) noexcept {
        if (srv == nullptr || w <= 0.0f || h <= 0.0f) return {};
        if (m_backbuffer_width == 0 || m_backbuffer_height == 0) return dh::unexpected{image_refusal::viewport_unavailable};
        if (m_pending_image_srvs.size() + 1 > k_max_images_per_frame) {
            ++m_dropped_images_this_frame;
            return dh::unexpected{image_refusal::frame_full};
        }
        const float viewport_width = authored_viewport_w();
        const float viewport_height = authored_viewport_h();
        if (viewport_width <= 0.0f || viewport_height <= 0.0f) return dh::unexpected{image_refusal::viewport_unavailable};
        const bool should_sort = order == image_order::batchable;
        if (m_segments.empty()) open_new_segment();
        draw_segment* segment = &m_segments.back();
        if (segment->image_begin == m_pending_image_srvs.size()) {
            segment->sort_images = should_sort;
        } else if (segment->sort_images != should_sort) {
            force_new_segment();
            segment = &m_segments.back();
            segment->sort_images = should_sort;
        }
        const float x0 = x / viewport_width * 2.0f - 1.0f;
        const float x1 = (x + w) / viewport_width * 2.0f - 1.0f;
        const float y0 = 1.0f - y / viewport_height * 2.0f;
        const float y1 = 1.0f - (y + h) / viewport_height * 2.0f;
        const dh::array<float, 4> tint_channels{tint.r_norm(), tint.g_norm(), tint.b_norm(), tint.a_norm() * m_alpha_multiplier};
        // hold the view through submission even if the caller releases its texture.
        m_pending_image_srvs.push_back(com_ptr<ID3D11ShaderResourceView>{srv});
        m_pending_image_verts.push_back({{x0, y0, 0.0f, 1.0f}, {u0, v0}, tint_channels});
        m_pending_image_verts.push_back({{x1, y0, 0.0f, 1.0f}, {u1, v0}, tint_channels});
        m_pending_image_verts.push_back({{x1, y1, 0.0f, 1.0f}, {u1, v1}, tint_channels});
        m_pending_image_verts.push_back({{x0, y0, 0.0f, 1.0f}, {u0, v0}, tint_channels});
        m_pending_image_verts.push_back({{x1, y1, 0.0f, 1.0f}, {u1, v1}, tint_channels});
        m_pending_image_verts.push_back({{x0, y1, 0.0f, 1.0f}, {u0, v1}, tint_channels});
        return {};
    }

    void d3d11_renderer::add_outlined_circle_aa(float cx, float cy, float radius, math::color c, float thickness, int segments) noexcept {
        if (radius <= 0.0f || thickness <= 0.0f || segments < 3) return;

        const dh::size_t need = static_cast<dh::size_t>(segments) * k_aa_line_vertices;
        if (m_pending_vertices.size() + need > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }
        const float step = math::k_two_pi / static_cast<float>(segments);
        float previous_x = cx + radius;
        float previous_y = cy;
        for (int index = 1; index <= segments; ++index) {
            const float angle = step * static_cast<float>(index);
            const float current_x = cx + dh::cos(angle) * radius;
            const float current_y = cy + dh::sin(angle) * radius;
            add_line_aa(previous_x, previous_y, current_x, current_y, c, thickness);
            previous_x = current_x;
            previous_y = current_y;
        }
    }

    void d3d11_renderer::add_outlined_rounded_rect_aa(float x, float y, float w, float h, float radius, math::color c, float thickness, int segments_per_corner) noexcept {
        if (w <= 0.0f || h <= 0.0f || thickness <= 0.0f || segments_per_corner < 1) return;
        radius = dh::clamp(radius, 0.0f, dh::min(w, h) * 0.5f);
        const int point_count = (segments_per_corner + 1) * 4;

        const dh::size_t need = static_cast<dh::size_t>(point_count) * k_aa_line_vertices;
        if (m_pending_vertices.size() + need > k_max_vertices_per_frame) {
            ++m_dropped_rects_this_frame;
            return;
        }

        auto previous = rounded_contour_point(x, y, w, h, radius, segments_per_corner, point_count - 1, radius);
        for (int index = 0; index < point_count; ++index) {
            const auto current = rounded_contour_point(x, y, w, h, radius, segments_per_corner, index, radius);
            add_line_aa(previous[0], previous[1], current[0], current[1], c, thickness);
            previous = current;
        }
    }

    void d3d11_renderer::add_image(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, math::color tint) noexcept {
        static_cast<void>(add_image_quad(srv, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, tint, image_order::batchable));
    }

    dh::expected<void, image_refusal> d3d11_renderer::add_image_region(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, float u0, float v0, float u1, float v1, math::color tint) noexcept {
        return add_image_quad(srv, x, y, w, h, u0, v0, u1, v1, tint, image_order::preserve);
    }

    void d3d11_renderer::add_text(float x, float y, dh::string_view str, math::color c) noexcept {
        add_text(k_body_font, x, y, str, c);
    }

    void d3d11_renderer::add_text(font_id id, float x, float y, dh::string_view str, math::color c) noexcept {
        add_text_layout(id, x, y, str, 0.0f, c);
    }

    void d3d11_renderer::add_text_truncated(float x, float y, dh::string_view str, float max_width, math::color c) noexcept {
        add_text_truncated(k_body_font, x, y, str, max_width, c);
    }

    void d3d11_renderer::add_text_truncated(font_id id, float x, float y, dh::string_view str, float max_width, math::color c) noexcept {
        if (!dh::isfinite(max_width)) {
            note_text_failure(id, {atlas_error::invalid_width, E_INVALIDARG});
            return;
        }
        if (max_width <= 0.0f) return;
        add_text_layout(id, x, y, str, max_width * m_dpi_scale, c);
    }

    void d3d11_renderer::note_text_failure(font_id id, atlas_failure failure) const noexcept {
        static_assert(static_cast<unsigned>(atlas_error::create_srv_failed) < sizeof(dh::u64) * 8);
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size()) return;
        const dh::u64 bit = dh::u64{1} << static_cast<unsigned>(failure.code);
        if (m_text_failure_logged[index] & bit) return;
        m_text_failure_logged[index] |= bit;
        if (failure.code == atlas_error::missing_glyph) {
            m_log.warn("text '{}': glyph missing from installed fonts, showing .notdef", m_fonts[index].name);
            return;
        }
        m_log.warn("text '{}' failed: {} hr={:#x}", m_fonts[index].name, to_string(failure.code), static_cast<unsigned>(failure.hr));
    }

    void d3d11_renderer::add_text_layout(font_id id, float x, float y, dh::string_view str, float max_width, math::color c) noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size() || str.empty() || !m_font_atlases[index].is_initialized()) return;
        if (!m_backbuffer_width || !m_backbuffer_height || !dh::isfinite(x) || !dh::isfinite(y)) return;
        const auto prepared = m_font_atlases[index].prepare(str, max_width);
        if (m_font_atlases[index].has_missing_glyphs()) note_text_failure(id, {atlas_error::missing_glyph, S_OK});
        if (!prepared) {
            note_text_failure(id, prepared.error());
            return;
        }
        const auto glyphs = *prepared;
        const auto style = m_fonts[index].style;

        constexpr dh::size_t k_outline_passes = 5;
        constexpr dh::size_t k_shadow_passes = 2;
        const dh::size_t passes = style == font_style::outline ? k_outline_passes : (style == font_style::dropshadow ? k_shadow_passes : 1);
        const dh::size_t needed = glyphs.size() * k_verts_per_glyph * passes;
        if (needed > k_max_text_vertices_per_frame - m_pending_text_by_font[index].size()) {
            m_dropped_glyphs_this_frame += glyphs.size();
            return;
        }
        const float offset = 1.0f / m_dpi_scale;
        const math::color shadow{0, 0, 0, c.a};
        switch (style) {
            case font_style::dropshadow:
                emit_text_run(index, x + offset, y + offset, glyphs, shadow);
                break;
            case font_style::outline:
                emit_text_run(index, x + offset, y, glyphs, shadow);
                emit_text_run(index, x - offset, y, glyphs, shadow);
                emit_text_run(index, x, y + offset, glyphs, shadow);
                emit_text_run(index, x, y - offset, glyphs, shadow);
                break;
            case font_style::none:
                break;
        }
        emit_text_run(index, x, y, glyphs, c);
    }

    dh::expected<void, atlas_failure> d3d11_renderer::submit_text(font_atlas& atlas, float x, float y, dh::string_view str, math::color c, font_style style) noexcept {
        return submit_text_flow(atlas, x, y, str, 0.0f, {text_flow::trim}, style, c);
    }

    dh::expected<void, atlas_failure> d3d11_renderer::submit_layout(font_atlas& atlas, float x, float y, dh::string_view str, float width, math::color c, text_layout_options options, font_style style) noexcept {
        return submit_text_flow(atlas, x, y, str, width, options, style, c);
    }

    dh::expected<void, atlas_failure> d3d11_renderer::submit_layout(font_id id, float x, float y, dh::string_view str, float width, math::color c, text_layout_options options) noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size()) return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        return submit_text_flow(m_font_atlases[index], x, y, str, width, options, m_fonts[index].style, c);
    }

    dh::expected<text_size, atlas_failure> d3d11_renderer::measure_layout(font_id id, dh::string_view str, float width, text_layout_options options) const noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size()) return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        const auto result = m_font_atlases[index].measure_layout(str, width * m_dpi_scale, options);
        if (!result) return dh::unexpected{result.error()};
        return text_size{result->width / m_dpi_scale, result->height / m_dpi_scale};
    }

    dh::expected<text_metrics, atlas_failure> d3d11_renderer::layout_metrics(font_id id, dh::string_view str, float width, text_layout_options options) const noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size()) return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        const auto result = m_font_atlases[index].layout_metrics(str, width * m_dpi_scale, options);
        if (!result) return dh::unexpected{result.error()};
        return result->scaled(1.0f / m_dpi_scale);
    }

    dh::expected<font_metrics, atlas_failure> d3d11_renderer::metrics(font_id id) const noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size()) return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        const auto result = m_font_atlases[index].metrics();
        if (!result) return dh::unexpected{result.error()};
        return result->scaled(1.0f / m_dpi_scale);
    }

    dh::expected<void, atlas_failure> d3d11_renderer::submit_text_flow(font_atlas& atlas, float x, float y, dh::string_view str, float width, text_layout_options options, font_style style, math::color c) noexcept {
        if (!m_initialized || !m_backbuffer_width || !m_backbuffer_height)
            return dh::unexpected{atlas_failure{atlas_error::not_initialized, E_FAIL}};
        if (!dh::isfinite(x) || !dh::isfinite(y))
            return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        if (atlas.device() != m_device.Get())
            return dh::unexpected{atlas_failure{atlas_error::device_mismatch, E_INVALIDARG}};
        if (style != font_style::none && style != font_style::dropshadow && style != font_style::outline)
            return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        const auto prepared = atlas.prepare(str, width * m_dpi_scale, options.flow, options.alignment);
        if (!prepared) return dh::unexpected{prepared.error()};
        const auto glyphs = *prepared;
        const dh::size_t passes = style == font_style::outline ? 5 : (style == font_style::dropshadow ? 2 : 1);
        if (glyphs.size() > (k_max_text_vertices_per_frame - m_pending_text_by_font[k_external_text_stream].size()) / (k_verts_per_glyph * passes))
            return dh::unexpected{atlas_failure{atlas_error::submission_full, E_BOUNDS}};
        if (glyphs.empty()) return {};

        force_new_segment();
        const float offset = 1.0f / m_dpi_scale;
        const math::color shadow{0, 0, 0, c.a};
        if (style == font_style::dropshadow) emit_text_run(k_external_text_stream, x + offset, y + offset, glyphs, shadow);
        else if (style == font_style::outline) {
            emit_text_run(k_external_text_stream, x + offset, y, glyphs, shadow);
            emit_text_run(k_external_text_stream, x - offset, y, glyphs, shadow);
            emit_text_run(k_external_text_stream, x, y + offset, glyphs, shadow);
            emit_text_run(k_external_text_stream, x, y - offset, glyphs, shadow);
        }
        emit_text_run(k_external_text_stream, x, y, glyphs, c);
        force_new_segment();
        return {};
    }

    d3d11_renderer::submission_mark d3d11_renderer::mark_submissions() const noexcept {
        submission_mark mark;
        mark.owner = this;
        mark.frame = m_submission_frame;
        mark.shapes = m_pending_vertices.size();
        mark.images = m_pending_image_verts.size();
        mark.checkers = m_pending_checker_verts.size();
        for (dh::size_t index = 0; index < k_text_stream_count; ++index) mark.text[index] = m_pending_text_by_font[index].size();
        return mark;
    }

    bool d3d11_renderer::current_submission_mark(const submission_mark& mark) const noexcept {
        if (!m_initialized || mark.owner != this || mark.frame != m_submission_frame || mark.shapes > m_pending_vertices.size() || mark.images > m_pending_image_verts.size() || mark.checkers > m_pending_checker_verts.size())
            return false;
        for (dh::size_t index = 0; index < k_text_stream_count; ++index)
            if (mark.text[index] > m_pending_text_by_font[index].size()) return false;
        return true;
    }

    dh::string_view to_string(opacity_error error) noexcept {
        switch (error) {
            case opacity_error::expired_scope: return "expired_scope";
            case opacity_error::invalid_opacity: return "invalid_opacity";
        }
        return "unknown";
    }

    dh::expected<dh::size_t, opacity_error> d3d11_renderer::opacity_submissions(const submission_mark& mark, float opacity) noexcept {
        if (!current_submission_mark(mark)) return dh::unexpected{opacity_error::expired_scope};
        if (!(opacity >= 0.0f && opacity <= 1.0f)) return dh::unexpected{opacity_error::invalid_opacity};
        dh::size_t count = 0;
        const auto apply = [&](auto& stream, dh::size_t first) noexcept {
            count += stream.size() - first;
            for (dh::size_t index = first; index < stream.size(); ++index) stream[index].color[3] *= opacity;
        };
        apply(m_pending_vertices, mark.shapes);
        apply(m_pending_image_verts, mark.images);
        apply(m_pending_checker_verts, mark.checkers);
        for (dh::size_t index = 0; index < k_text_stream_count; ++index) apply(m_pending_text_by_font[index], mark.text[index]);
        return count;
    }

    void d3d11_renderer::discard_submissions(const submission_mark& mark) noexcept {
        if (!current_submission_mark(mark)) dh::fail_fast("renderer: cannot discard an expired submission mark", FAST_FAIL_INVALID_ARG);
        m_pending_vertices.resize(mark.shapes);
        m_pending_image_verts.resize(mark.images);
        m_pending_image_srvs.resize(mark.images / k_verts_per_rect);
        m_pending_checker_verts.resize(mark.checkers);
        for (dh::size_t index = 0; index < k_text_stream_count; ++index) {
            m_pending_text_by_font[index].resize(mark.text[index]);
            m_pending_text_pages[index].resize(mark.text[index] / k_verts_per_glyph);
        }
        m_external_text_pages.resize(mark.text[k_external_text_stream] / k_verts_per_glyph);
        for (auto& segment : m_segments) {
            segment.rect_begin = dh::min(segment.rect_begin, mark.shapes);
            segment.rect_end = dh::min(segment.rect_end, mark.shapes);
            segment.image_begin = dh::min(segment.image_begin, m_pending_image_srvs.size());
            segment.image_end = dh::min(segment.image_end, m_pending_image_srvs.size());
            segment.checker_begin = dh::min(segment.checker_begin, mark.checkers);
            segment.checker_end = dh::min(segment.checker_end, mark.checkers);
            for (dh::size_t index = 0; index < k_text_stream_count; ++index) {
                segment.text_begin[index] = dh::min(segment.text_begin[index], mark.text[index]);
                segment.text_end[index] = dh::min(segment.text_end[index], mark.text[index]);
            }
        }
    }

    dh::expected<dh::size_t, transform_error> d3d11_renderer::transform_submissions(const submission_mark& mark, const affine_transform& transform) noexcept {
        if (!current_submission_mark(mark)) return dh::unexpected{transform_error::expired_scope};
        const auto finite = [](double value) noexcept {
            constexpr double bound = dh::numeric_limits<double>::max();
            return value >= -bound && value <= bound;
        };
        if (!finite(transform.xx) || !finite(transform.xy) || !finite(transform.yx) || !finite(transform.yy) || !finite(transform.x) || !finite(transform.y))
            return dh::unexpected{transform_error::invalid_transform};
        const double width = authored_viewport_w(), height = authored_viewport_h();
        if (!(width > 0.0 && height > 0.0)) return dh::unexpected{transform_error::expired_scope};
        const auto mapped = [&](const dh::array<float, 4>& position) noexcept {
            const double x = (static_cast<double>(position[0]) + 1.0) * width * 0.5;
            const double y = (1.0 - static_cast<double>(position[1])) * height * 0.5;
            return dh::array<double, 2>{(transform.xx * x + transform.xy * y + transform.x) / width * 2.0 - 1.0,
                1.0 - (transform.yx * x + transform.yy * y + transform.y) / height * 2.0};
        };
        const auto fits = [&](const auto& stream, dh::size_t first) noexcept {
            constexpr double bound = dh::numeric_limits<float>::max();
            for (dh::size_t index = first; index < stream.size(); ++index) {
                const auto point = mapped(stream[index].pos);
                if (!(point[0] >= -bound && point[0] <= bound && point[1] >= -bound && point[1] <= bound)) return false;
            }
            return true;
        };

        const float determinant = static_cast<float>(transform.xx * transform.yy - transform.xy * transform.yx);
        const double grid_scale = dh::sqrt(dh::fabs(determinant));
        const auto mapped_grid = [&](const dh::array<float, 4>& grid) noexcept {
            const double x = grid[0], y = grid[1];
            return dh::array<double, 3>{transform.xx * x + transform.xy * y + transform.x, transform.yx * x + transform.yy * y + transform.y,
                static_cast<double>(grid[2]) * grid_scale};
        };
        const auto grid_fits = [&]() noexcept {
            constexpr double bound = dh::numeric_limits<float>::max();
            for (dh::size_t index = mark.checkers; index < m_pending_checker_verts.size(); ++index) {
                const auto grid = mapped_grid(m_pending_checker_verts[index].grid);
                for (const double value : grid)
                    if (!(value >= -bound && value <= bound)) return false;
                if (!(grid[2] >= 0.0)) return false;
            }
            return true;
        };
        bool valid = fits(m_pending_vertices, mark.shapes) && fits(m_pending_image_verts, mark.images) &&
            fits(m_pending_checker_verts, mark.checkers) && grid_fits();
        for (dh::size_t index = 0; index < k_text_stream_count; ++index) valid = valid && fits(m_pending_text_by_font[index], mark.text[index]);
        if (!valid) {
            discard_submissions(mark);
            return dh::unexpected{transform_error::coordinate_overflow};
        }
        dh::size_t count = 0;
        const auto apply = [&](auto& stream, dh::size_t first) noexcept {
            count += stream.size() - first;
            for (dh::size_t index = first; index < stream.size(); ++index) {
                const auto point = mapped(stream[index].pos);
                stream[index].pos[0] = static_cast<float>(point[0]);
                stream[index].pos[1] = static_cast<float>(point[1]);
            }
        };
        apply(m_pending_vertices, mark.shapes);
        apply(m_pending_image_verts, mark.images);
        apply(m_pending_checker_verts, mark.checkers);
        for (dh::size_t index = mark.checkers; index < m_pending_checker_verts.size(); ++index) {
            auto& grid = m_pending_checker_verts[index].grid;
            const auto moved = mapped_grid(grid);
            grid[0] = static_cast<float>(moved[0]);
            grid[1] = static_cast<float>(moved[1]);
            grid[2] = static_cast<float>(moved[2]);
        }
        for (dh::size_t index = 0; index < k_text_stream_count; ++index) apply(m_pending_text_by_font[index], mark.text[index]);
        return count;
    }

    void d3d11_renderer::emit_text_run(dh::size_t index, float x, float y, dh::span<const text_glyph> glyphs, math::color c) noexcept {
        auto& vertices = m_pending_text_by_font[index];
        auto& pages = m_pending_text_pages[index];
        const float width = static_cast<float>(m_backbuffer_width);
        const float height = static_cast<float>(m_backbuffer_height);
        const float origin_x = to_device_origin(x);
        const float origin_y = to_device_origin(y);
        const dh::array<float, 4> color{c.r_norm(), c.g_norm(), c.b_norm(), c.a_norm() * m_alpha_multiplier};
        for (const auto& glyph : glyphs) {
            const float left = origin_x + glyph.left;
            const float top = origin_y + glyph.top;
            const float x0 = left / width * 2.0f - 1.0f;
            const float y0 = 1.0f - top / height * 2.0f;
            const float x1 = (left + static_cast<float>(glyph.width)) / width * 2.0f - 1.0f;
            const float y1 = 1.0f - (top + static_cast<float>(glyph.height)) / height * 2.0f;
            pages.push_back(glyph.srv);
            if (index == k_external_text_stream) m_external_text_pages.push_back(com_ptr<ID3D11ShaderResourceView>{glyph.srv});
            vertices.push_back({{x0, y0, 0.0f, 1.0f}, {glyph.u0, glyph.v0}, color});
            vertices.push_back({{x1, y0, 0.0f, 1.0f}, {glyph.u1, glyph.v0}, color});
            vertices.push_back({{x1, y1, 0.0f, 1.0f}, {glyph.u1, glyph.v1}, color});
            vertices.push_back({{x0, y0, 0.0f, 1.0f}, {glyph.u0, glyph.v0}, color});
            vertices.push_back({{x1, y1, 0.0f, 1.0f}, {glyph.u1, glyph.v1}, color});
            vertices.push_back({{x0, y1, 0.0f, 1.0f}, {glyph.u0, glyph.v1}, color});
        }
    }

    float d3d11_renderer::measure_text_width(dh::string_view str) const noexcept {
        return measure_text_width(k_body_font, str);
    }

    float d3d11_renderer::measure_text_width(font_id id, dh::string_view str) const noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size() || str.empty() || !m_font_atlases[index].is_initialized()) return 0.0f;
        const auto measured = m_font_atlases[index].measure(str);
        if (m_font_atlases[index].has_missing_glyphs()) note_text_failure(id, {atlas_error::missing_glyph, S_OK});
        if (!measured) {
            note_text_failure(id, measured.error());
            return 0.0f;
        }
        return *measured / m_dpi_scale;
    }

    int d3d11_renderer::text_line_height() const noexcept {
        return text_line_height(k_body_font);
    }

    dh::expected<text_caret, atlas_failure> d3d11_renderer::trailing_text_caret(font_id id, dh::string_view text) const noexcept {
        const auto index = static_cast<dh::size_t>(id);
        if (index >= m_fonts.size() || !m_font_atlases[index].is_initialized()) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        const auto caret = m_font_atlases[index].trailing_caret(text);
        if (!caret) { note_text_failure(id, caret.error()); return dh::unexpected{caret.error()}; }
        return caret->scaled(1.0f / m_dpi_scale);
    }

    int d3d11_renderer::text_line_height(font_id id) const noexcept {
        const auto fid = static_cast<dh::size_t>(id);
        if (fid >= m_fonts.size()) return 0;
        return static_cast<int>(static_cast<float>(m_font_atlases[fid].line_height()) / m_dpi_scale);
    }

    int d3d11_renderer::text_ascent(font_id id) const noexcept {
        const auto fid = static_cast<dh::size_t>(id);
        if (fid >= m_fonts.size()) return 0;
        return static_cast<int>(static_cast<float>(m_font_atlases[fid].ascent()) / m_dpi_scale);
    }

    void d3d11_renderer::release_render_target() noexcept {
        m_rtv.Reset();
    }

    dh::expected<void, render_error> d3d11_renderer::acquire_render_target(IDXGISwapChain* swapchain) noexcept {
        if (swapchain == nullptr) return dh::unexpected{render_error::null_swapchain};
        if (!m_device) return dh::unexpected{render_error::get_device_failed};

        ID3D11Texture2D* backbuffer_raw = nullptr;
        HRESULT hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer_raw));
        com_ptr<ID3D11Texture2D> backbuffer;
        backbuffer.Attach(backbuffer_raw);
        if (FAILED(hr) || !backbuffer) {
            return dh::unexpected{render_error::get_buffer_failed};
        }

        hr = m_device->CreateRenderTargetView(backbuffer.Get(), nullptr, m_rtv.ReleaseAndGetAddressOf());
        if (FAILED(hr) || !m_rtv) {
            return dh::unexpected{render_error::create_rtv_failed};
        }

        if (auto dim = capture_swapchain_dimensions(swapchain); !dim) {
            return dh::unexpected{dim.error()};
        }
        return {};
    }

    void d3d11_renderer::shutdown() noexcept {
        for (auto& pages : m_pending_text_pages)
            pages.clear();
        m_external_text_pages.clear();
        m_text_failure_logged = {};
        for (auto& a : m_font_atlases)
            a.shutdown();

        m_text_sampler.Reset();
        for (auto& b : m_text_vertex_buffers)
            b.Reset();
        m_text_input_layout.Reset();
        m_text_pixel_shader.Reset();
        m_image_pixel_shader.Reset();
        m_checker_pixel_shader.Reset();
        m_checker_vertex_shader.Reset();
        m_checker_input_layout.Reset();
        m_text_vertex_shader.Reset();

        m_rtv.Reset();
        m_vertex_buffer.Reset();
        m_input_layout.Reset();
        m_pixel_shader.Reset();
        m_vertex_shader.Reset();
        m_blend_state.Reset();
        m_rasterizer_state.Reset();
        m_context.Reset();
        m_image_vertex_buffer.Reset();
        m_checker_vertex_buffer.Reset();
        m_device.Reset();

        m_pending_vertices.clear();
        for (auto& v : m_pending_text_by_font)
            v.clear();
        m_pending_image_verts.clear();
        m_pending_image_srvs.clear();
        m_pending_checker_verts.clear();

        m_initialized = false;
        m_permanent_failure.reset();
        m_init_failure_count = 0;
        m_map_failed_logged = false;
        m_initialized_swapchain = nullptr;
        m_dropped_rects_this_frame = 0;
        m_overflow_warned = false;
        m_dropped_glyphs_this_frame = 0;
        m_text_overflow_warned = false;
        m_dropped_images_this_frame = 0;
        m_image_overflow_warned = false;
        m_dropped_checkers_this_frame = 0;
        m_checker_overflow_warned = false;
    }

    dh::expected<void, render_error> d3d11_renderer::initialize(IDXGISwapChain* swapchain) noexcept {
        if (swapchain == nullptr) return dh::unexpected{render_error::null_swapchain};

        ID3D11Device* device_raw = nullptr;
        HRESULT hr = swapchain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device_raw));
        m_device.Attach(device_raw);
        if (FAILED(hr) || !m_device) {
            return dh::unexpected{render_error::get_device_failed};
        }
        m_device->GetImmediateContext(m_context.ReleaseAndGetAddressOf());

        if (auto rtv = acquire_render_target(swapchain); !rtv) {
            return dh::unexpected{rtv.error()};
        }

        if (auto pipeline = create_pipeline_state(); !pipeline) {
            return dh::unexpected{pipeline.error()};
        }

        bake_fonts();

        m_pending_vertices.reserve(k_max_vertices_per_frame);
        for (auto& v : m_pending_text_by_font) {
            v.reserve(k_max_text_vertices_per_frame);
        }

        m_pending_image_verts.reserve(k_max_images_per_frame * k_verts_per_image);
        m_pending_image_srvs.reserve(k_max_images_per_frame);
        m_image_sort_indices.reserve(k_max_images_per_frame);
        m_image_sort_verts_scratch.reserve(k_max_images_per_frame * k_verts_per_image);
        m_image_sort_srvs_scratch.reserve(k_max_images_per_frame);
        m_pending_checker_verts.reserve(k_max_checker_verts_per_frame);

        m_initialized = true;
        m_log.info("renderer ready: {}x{}", m_backbuffer_width, m_backbuffer_height);
        return {};
    }

    dh::expected<void, render_error> d3d11_renderer::create_pipeline_state() noexcept {
        HRESULT hr = m_device->CreateVertexShader(g_overlay_vs_bytecode, sizeof(g_overlay_vs_bytecode), nullptr, m_vertex_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};

        hr = m_device->CreatePixelShader(g_overlay_ps_bytecode, sizeof(g_overlay_ps_bytecode), nullptr, m_pixel_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};

        constexpr D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        hr = m_device->CreateInputLayout(layout, static_cast<UINT>(dh::size(layout)), g_overlay_vs_bytecode, sizeof(g_overlay_vs_bytecode), m_input_layout.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_input_layout_failed};

        D3D11_BUFFER_DESC vb_desc{};
        vb_desc.Usage = D3D11_USAGE_DYNAMIC;
        vb_desc.ByteWidth = static_cast<UINT>(sizeof(vertex) * k_max_vertices_per_frame);
        vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&vb_desc, nullptr, m_vertex_buffer.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_buffer_failed};

        D3D11_BLEND_DESC blend_desc{};
        blend_desc.RenderTarget[0].BlendEnable = TRUE;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&blend_desc, m_blend_state.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_blend_state_failed};

        D3D11_RASTERIZER_DESC rs_desc{};
        rs_desc.FillMode = D3D11_FILL_SOLID;
        rs_desc.CullMode = D3D11_CULL_NONE;
        rs_desc.DepthClipEnable = TRUE;

        rs_desc.ScissorEnable = TRUE;

        hr = m_device->CreateRasterizerState(&rs_desc, m_rasterizer_state.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_rasterizer_failed};

        hr = m_device->CreateVertexShader(g_overlay_text_vs_bytecode, sizeof(g_overlay_text_vs_bytecode), nullptr, m_text_vertex_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};

        hr = m_device->CreatePixelShader(g_overlay_text_ps_bytecode, sizeof(g_overlay_text_ps_bytecode), nullptr, m_text_pixel_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};
        hr = m_device->CreatePixelShader(g_overlay_image_ps_bytecode, sizeof(g_overlay_image_ps_bytecode), nullptr, m_image_pixel_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};
        hr = m_device->CreateVertexShader(g_overlay_checker_vs_bytecode, sizeof(g_overlay_checker_vs_bytecode), nullptr, m_checker_vertex_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};
        hr = m_device->CreatePixelShader(g_overlay_checker_ps_bytecode, sizeof(g_overlay_checker_ps_bytecode), nullptr, m_checker_pixel_shader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_shader_failed};
        constexpr D3D11_INPUT_ELEMENT_DESC checker_layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(checker_vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(checker_vertex, grid), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(checker_vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        hr = m_device->CreateInputLayout(checker_layout, static_cast<UINT>(dh::size(checker_layout)), g_overlay_checker_vs_bytecode, sizeof(g_overlay_checker_vs_bytecode), m_checker_input_layout.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_input_layout_failed};

        constexpr D3D11_INPUT_ELEMENT_DESC text_layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(text_vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(text_vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(text_vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        hr = m_device->CreateInputLayout(text_layout, static_cast<UINT>(dh::size(text_layout)), g_overlay_text_vs_bytecode, sizeof(g_overlay_text_vs_bytecode), m_text_input_layout.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_input_layout_failed};

        D3D11_BUFFER_DESC text_vb_desc{};
        text_vb_desc.Usage = D3D11_USAGE_DYNAMIC;
        text_vb_desc.ByteWidth = static_cast<UINT>(sizeof(text_vertex) * k_max_text_vertices_per_frame);
        text_vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        text_vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        for (auto& buf : m_text_vertex_buffers) {
            hr = m_device->CreateBuffer(&text_vb_desc, nullptr, buf.ReleaseAndGetAddressOf());
            if (FAILED(hr)) return dh::unexpected{render_error::create_buffer_failed};
        }

        D3D11_SAMPLER_DESC sampler_desc{};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.MinLOD = 0.0f;
        sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&sampler_desc, m_text_sampler.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_sampler_failed};

        D3D11_BUFFER_DESC img_vb_desc{};
        img_vb_desc.Usage = D3D11_USAGE_DYNAMIC;
        img_vb_desc.ByteWidth = static_cast<UINT>(sizeof(text_vertex) * k_max_image_verts_per_frame);
        img_vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        img_vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&img_vb_desc, nullptr, m_image_vertex_buffer.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_buffer_failed};

        D3D11_BUFFER_DESC checker_vb_desc = img_vb_desc;
        checker_vb_desc.ByteWidth = static_cast<UINT>(sizeof(checker_vertex) * k_max_checker_verts_per_frame);
        hr = m_device->CreateBuffer(&checker_vb_desc, nullptr, m_checker_vertex_buffer.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return dh::unexpected{render_error::create_buffer_failed};

        return {};
    }

    dh::expected<void, render_error> d3d11_renderer::capture_swapchain_dimensions(IDXGISwapChain* swapchain) noexcept {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(swapchain->GetDesc(&desc))) {
            return dh::unexpected{render_error::get_desc_failed};
        }
        m_backbuffer_width = desc.BufferDesc.Width;
        m_backbuffer_height = desc.BufferDesc.Height;
        return {};
    }

}
