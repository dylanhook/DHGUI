// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "font_atlas.h"
#include "font_table.h"
#include "path_geometry.h"
#include "log/scoped_logger.h"
#include "math/color.h"

#include "core/dh/algorithm.h"
#include "core/dh/expected.h"
#include "core/dh/optional.h"
#include "core/dh/vector.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "core/dh/array.h"
#include "core/dh/string.h"
#include "core/dh/memory.h"

namespace dh::render {

    enum class render_error {
        null_swapchain,
        get_device_failed,
        get_buffer_failed,
        get_desc_failed,
        create_rtv_failed,
        create_buffer_failed,
        create_shader_failed,
        create_input_layout_failed,
        create_blend_state_failed,
        create_rasterizer_failed,
        create_sampler_failed,
        map_buffer_failed,
        device_removed,

        render_target_absent,

        font_table_invalid,
    };

    [[nodiscard]] dh::string_view to_string(render_error e) noexcept;

    enum class transform_error : dh::u8 { invalid_transform, expired_scope, coordinate_overflow };
    [[nodiscard]] dh::string_view to_string(transform_error error) noexcept;
    enum class opacity_error : dh::u8 { expired_scope, invalid_opacity };
    [[nodiscard]] dh::string_view to_string(opacity_error error) noexcept;

    enum class image_refusal : dh::u8 { frame_full, viewport_unavailable };
    [[nodiscard]] dh::string_view to_string(image_refusal refusal) noexcept;

    struct affine_transform final {
    public:
        constexpr affine_transform() noexcept = default;
        ~affine_transform() = default;
        affine_transform(const affine_transform&) = default;
        affine_transform& operator=(const affine_transform&) = default;
        affine_transform(affine_transform&&) noexcept = default;
        affine_transform& operator=(affine_transform&&) noexcept = default;
        double xx{1.0}, xy{0.0}, yx{0.0}, yy{1.0}, x{0.0}, y{0.0};
    };

    class d3d11_renderer final {
    public:
        static constexpr dh::size_t k_external_text_stream = k_max_fonts;
        static constexpr dh::size_t k_text_stream_count = k_max_fonts + 1;

        struct submission_mark final {
        public:
            submission_mark() noexcept = default;
            ~submission_mark() = default;
            submission_mark(const submission_mark&) = default;
            submission_mark& operator=(const submission_mark&) = default;
            submission_mark(submission_mark&&) noexcept = default;
            submission_mark& operator=(submission_mark&&) noexcept = default;
            const d3d11_renderer* owner{nullptr};
            dh::u64 frame{0};
            dh::size_t shapes{0}, images{0}, checkers{0};
            dh::array<dh::size_t, k_text_stream_count> text{};
        };

        d3d11_renderer(log::scoped_logger log, dh::span<const font_face> fonts) noexcept;
        ~d3d11_renderer();

        d3d11_renderer(const d3d11_renderer&) = delete;
        d3d11_renderer& operator=(const d3d11_renderer&) = delete;
        d3d11_renderer(d3d11_renderer&&) = delete;
        d3d11_renderer& operator=(d3d11_renderer&&) = delete;

        [[nodiscard]] dh::expected<void, render_error> begin_frame(IDXGISwapChain* swapchain) noexcept;
        void end_frame() noexcept;

        [[nodiscard]] submission_mark mark_submissions() const noexcept;

        void discard_submissions(const submission_mark& mark) noexcept;
        [[nodiscard]] dh::expected<dh::size_t, transform_error> transform_submissions(const submission_mark& mark, const affine_transform& transform) noexcept;
        [[nodiscard]] dh::expected<dh::size_t, opacity_error> opacity_submissions(const submission_mark& mark, float opacity) noexcept;

        void add_filled_rect(float x, float y, float w, float h, math::color c) noexcept;
        void add_outlined_rect(float x, float y, float w, float h, math::color c, float thickness = 1.0f) noexcept;
        void add_filled_rounded_rect(float x, float y, float w, float h, float radius, math::color c, int segments_per_corner = 12) noexcept;

        void add_outlined_rounded_rect_aa(float x, float y, float w, float h, float radius, math::color c, float thickness = 1.0f, int segments_per_corner = 12) noexcept;

        void add_filled_circle(float cx, float cy, float radius, math::color c, int segments = 48) noexcept;
        void add_outlined_circle_aa(float cx, float cy, float radius, math::color c, float thickness = 1.0f, int segments = 48) noexcept;

        void add_filled_triangle_aa(float x1, float y1, float x2, float y2, float x3, float y3, math::color c) noexcept {
            add_filled_triangle_aa(x1, y1, x2, y2, x3, y3, dh::array<math::color, 3>{c, c, c});
        }

        void add_filled_triangle_aa(float x1, float y1, float x2, float y2, float x3, float y3, const dh::array<math::color, 3>& colors) noexcept;

        void add_gradient_triangle(float x1, float y1, float x2, float y2, float x3, float y3, const dh::array<math::color, 3>& colors) noexcept;

        void add_line(float x1, float y1, float x2, float y2, math::color c, float thickness = 1.0f) noexcept;

        void add_line_aa(float x1, float y1, float x2, float y2, math::color c, float thickness = 1.0f) noexcept;

        void add_gradient_rect_v(float x, float y, float w, float h, math::color top_c, math::color bottom_c) noexcept;
        void add_gradient_rect(float x, float y, float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept;

        void add_checker_rect(float x, float y, float w, float h, float cell, math::color c) noexcept;
        [[nodiscard]] dh::expected<dh::size_t, geometry_failure> submit_gradient(float x, float y, float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept;
        [[nodiscard]] static dh::expected<dh::size_t, geometry_failure> gradient_vertices(float width, float height, math::color top_left, math::color top_right, math::color bottom_left, math::color bottom_right) noexcept;
        [[nodiscard]] dh::expected<dh::size_t, geometry_failure> submit_geometry(dh::span<const D2D1_TRIANGLE> triangles, float x, float y, math::color color) noexcept;

        void add_text(float x, float y, dh::string_view str, math::color c) noexcept;
        void add_text(font_id id, float x, float y, dh::string_view str, math::color c) noexcept;

        [[nodiscard]] dh::expected<void, atlas_failure> submit_text(font_atlas& atlas, float x, float y, dh::string_view str, math::color c, font_style style = font_style::none) noexcept;
        [[nodiscard]] dh::expected<void, atlas_failure> submit_layout(font_atlas& atlas, float x, float y, dh::string_view str, float width, math::color c, text_layout_options options = {}, font_style style = font_style::none) noexcept;
        [[nodiscard]] dh::expected<void, atlas_failure> submit_layout(font_id id, float x, float y, dh::string_view str, float width, math::color c, text_layout_options options = {}) noexcept;
        [[nodiscard]] dh::expected<text_size, atlas_failure> measure_layout(font_id id, dh::string_view str, float width, text_layout_options options = {}) const noexcept;
        [[nodiscard]] dh::expected<text_metrics, atlas_failure> layout_metrics(font_id id, dh::string_view str, float width, text_layout_options options = {}) const noexcept;
        [[nodiscard]] dh::expected<font_metrics, atlas_failure> metrics(font_id id) const noexcept;
        void add_text_truncated(float x, float y, dh::string_view str, float max_width, math::color c) noexcept;
        void add_text_truncated(font_id id, float x, float y, dh::string_view str, float max_width, math::color c) noexcept;

    private:
        [[nodiscard]] bool current_submission_mark(const submission_mark& mark) const noexcept;
        enum class image_order : dh::u8 { batchable, preserve };
        [[nodiscard]] dh::expected<void, image_refusal> add_image_quad(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, float u0, float v0, float u1, float v1, math::color tint, image_order order) noexcept;

        void add_text_layout(font_id id, float x, float y, dh::string_view str, float max_width, math::color c) noexcept;
        void emit_text_run(dh::size_t stream, float x, float y, dh::span<const text_glyph> glyphs, math::color c) noexcept;
        void note_text_failure(font_id id, atlas_failure failure) const noexcept;
        [[nodiscard]] dh::expected<void, atlas_failure> submit_text_flow(font_atlas& atlas, float x, float y, dh::string_view str, float width, text_layout_options options, font_style style, math::color c) noexcept;

    public:
        [[nodiscard]] float measure_text_width(dh::string_view str) const noexcept;
        [[nodiscard]] int text_line_height() const noexcept;
        [[nodiscard]] float measure_text_width(font_id id, dh::string_view str) const noexcept;
        [[nodiscard]] dh::expected<text_caret, atlas_failure> trailing_text_caret(font_id id, dh::string_view text) const noexcept;
        [[nodiscard]] int text_line_height(font_id id) const noexcept;
        [[nodiscard]] int text_ascent(font_id id) const noexcept;

        // add_image keeps a reference to the view until the frame is submitted.
        void add_image(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, math::color tint) noexcept;

        [[nodiscard]] dh::expected<void, image_refusal> add_image_region(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, float u0, float v0, float u1, float v1, math::color tint) noexcept;

        void set_alpha_multiplier(float m) noexcept;

        void push_clip_rect(float x, float y, float w, float h) noexcept;
        void pop_clip_rect() noexcept;

        void force_new_segment() noexcept;

        [[nodiscard]] int viewport_width() const noexcept;
        [[nodiscard]] int viewport_height() const noexcept;

        [[nodiscard]] int backbuffer_width() const noexcept { return static_cast<int>(m_backbuffer_width); }

        [[nodiscard]] int backbuffer_height() const noexcept { return static_cast<int>(m_backbuffer_height); }

        void set_dpi_scale(float scale) noexcept;

        [[nodiscard]] float dpi_scale() const noexcept { return m_dpi_scale; }

        static constexpr float k_min_dpi_scale = 0.25f;
        static constexpr float k_max_dpi_scale = 6.0f;

        void release_render_target() noexcept;

        [[nodiscard]] dh::expected<void, render_error> acquire_render_target(IDXGISwapChain* swapchain) noexcept;

        void shutdown() noexcept;

        [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }

        [[nodiscard]] dh::span<const font_face> fonts() const noexcept { return m_fonts; }

        [[nodiscard]] ID3D11Device* device() const noexcept { return m_device.Get(); }

    private:
        struct vertex {
            dh::array<float, 4> pos;
            dh::array<float, 4> color;
        };

        struct text_vertex {
            dh::array<float, 4> pos;
            dh::array<float, 2> uv;
            dh::array<float, 4> color;
        };

        struct checker_vertex {
            dh::array<float, 4> pos;
            dh::array<float, 4> grid;
            dh::array<float, 4> color;
        };

        [[nodiscard]] dh::expected<void, render_error> initialize(IDXGISwapChain* swapchain) noexcept;

        [[nodiscard]] dh::expected<void, render_error> create_pipeline_state() noexcept;

        [[nodiscard]] dh::expected<void, render_error> capture_swapchain_dimensions(IDXGISwapChain* swapchain) noexcept;

        void bake_fonts() noexcept;

        [[nodiscard]] float authored_viewport_w() const noexcept;
        [[nodiscard]] float authored_viewport_h() const noexcept;

        [[nodiscard]] float to_device_edge(float authored) const noexcept;

        [[nodiscard]] float to_device_origin(float authored) const noexcept;

        [[nodiscard]] float stroke_thickness(float authored) const noexcept;

        void emit_filled_rect_verts(float x, float y, float w, float h, math::color c) noexcept;

        [[nodiscard]] dh::expected<void, render_error> upload_to_buffer(ID3D11Buffer* buffer, const void* src, dh::size_t bytes) noexcept;

        template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>;

        log::scoped_logger m_log;

        dh::span<const font_face> m_fonts;

        dh::optional<font_table_error> m_font_table_failure{};
        bool m_initialized{false};

        dh::optional<render_error> m_permanent_failure{};

        dh::size_t m_init_failure_count{0};
        bool m_map_failed_logged{false};

        IDXGISwapChain* m_initialized_swapchain{nullptr};

        dh::size_t m_dropped_rects_this_frame{0};
        bool m_overflow_warned{false};
        dh::size_t m_dropped_glyphs_this_frame{0};
        bool m_text_overflow_warned{false};

        com_ptr<ID3D11Device> m_device;
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<ID3D11RenderTargetView> m_rtv;
        com_ptr<ID3D11VertexShader> m_vertex_shader;
        com_ptr<ID3D11PixelShader> m_pixel_shader;
        com_ptr<ID3D11InputLayout> m_input_layout;
        com_ptr<ID3D11Buffer> m_vertex_buffer;
        com_ptr<ID3D11BlendState> m_blend_state;
        com_ptr<ID3D11RasterizerState> m_rasterizer_state;

        dh::array<font_atlas, k_max_fonts> m_font_atlases;

        dh::array<com_ptr<ID3D11Buffer>, k_text_stream_count> m_text_vertex_buffers;
        com_ptr<ID3D11VertexShader> m_text_vertex_shader;
        com_ptr<ID3D11PixelShader> m_text_pixel_shader;
        com_ptr<ID3D11PixelShader> m_image_pixel_shader;
        com_ptr<ID3D11InputLayout> m_text_input_layout;
        com_ptr<ID3D11SamplerState> m_text_sampler;

        com_ptr<ID3D11Buffer> m_image_vertex_buffer;
        dh::vector<text_vertex> m_pending_image_verts;
        dh::vector<com_ptr<ID3D11ShaderResourceView>> m_pending_image_srvs;

        dh::vector<dh::size_t> m_image_sort_indices;
        dh::vector<text_vertex> m_image_sort_verts_scratch;
        dh::vector<com_ptr<ID3D11ShaderResourceView>> m_image_sort_srvs_scratch;
        dh::size_t m_dropped_images_this_frame{0};
        bool m_image_overflow_warned{false};

        com_ptr<ID3D11VertexShader> m_checker_vertex_shader;
        com_ptr<ID3D11PixelShader> m_checker_pixel_shader;
        com_ptr<ID3D11InputLayout> m_checker_input_layout;
        com_ptr<ID3D11Buffer> m_checker_vertex_buffer;
        dh::vector<checker_vertex> m_pending_checker_verts;
        dh::size_t m_dropped_checkers_this_frame{0};
        bool m_checker_overflow_warned{false};

        dh::u32 m_backbuffer_width{0};
        dh::u32 m_backbuffer_height{0};

        float m_dpi_scale{1.0f};
        dh::vector<vertex> m_pending_vertices;
        dh::array<dh::vector<text_vertex>, k_text_stream_count> m_pending_text_by_font;
        dh::array<dh::vector<ID3D11ShaderResourceView*>, k_text_stream_count> m_pending_text_pages;
        dh::vector<com_ptr<ID3D11ShaderResourceView>> m_external_text_pages;

        mutable dh::array<dh::u64, k_max_fonts> m_text_failure_logged{};
        float m_alpha_multiplier{1.0f};
        dh::u64 m_submission_frame{0};

        struct clip_rect {
            float x{0.0f};
            float y{0.0f};
            float w{0.0f};
            float h{0.0f};
        };

        struct draw_segment {
            clip_rect clip{};
            bool has_clip{false};
            dh::size_t rect_begin{0};
            dh::size_t rect_end{0};

            dh::array<dh::size_t, k_text_stream_count> text_begin{};
            dh::array<dh::size_t, k_text_stream_count> text_end{};

            dh::size_t image_begin{0};
            dh::size_t image_end{0};

            dh::size_t checker_begin{0};
            dh::size_t checker_end{0};

            bool sort_images{true};
        };

        dh::vector<draw_segment> m_segments;
        dh::vector<clip_rect> m_clip_stack;

        void close_current_segment() noexcept;
        void open_new_segment() noexcept;
        void reset_segments() noexcept;
    };

}
