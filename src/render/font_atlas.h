// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/string.h"
#include "core/dh/span.h"
#include "core/dh/unique_ptr.h"

#include <d3d11.h>

namespace dh::render {

    enum class atlas_error {
        not_initialized,
        allocation_failed,
        invalid_config,
        create_factory_failed,
        font_file_invalid,
        font_loader_failed,
        font_loader_register_failed,
        font_file_create_failed,
        font_set_failed,
        get_font_collection_failed,
        face_name_too_long,
        font_not_found,
        create_face_failed,
        face_reference_failed,
        compute_origins_failed,
        create_format_failed,
        configure_format_failed,
        create_ellipsis_failed,
        text_too_long,
        invalid_utf8,
        invalid_width,
        classify_direction_failed,
        create_layout_failed,
        configure_layout_failed,
        layout_metrics_failed,
        draw_layout_failed,
        unsupported_decoration,
        missing_glyph,
        create_glyph_run_analysis_failed,
        glyph_texture_bounds_failed,
        create_alpha_texture_failed,
        raster_too_large,
        cache_full,
        submission_full,
        device_mismatch,
        create_texture_failed,
        create_srv_failed,
        hit_test_failed,
    };

    [[nodiscard]] dh::string_view to_string(atlas_error error) noexcept;

    struct atlas_failure final {
        constexpr atlas_failure() noexcept = default;

        constexpr atlas_failure(atlas_error cause, HRESULT result) noexcept : code(cause), hr(result) {}

        ~atlas_failure() = default;
        atlas_failure(const atlas_failure&) noexcept = default;
        atlas_failure& operator=(const atlas_failure&) noexcept = default;
        atlas_failure(atlas_failure&&) noexcept = default;
        atlas_failure& operator=(atlas_failure&&) noexcept = default;
        atlas_error code{};
        HRESULT hr{};
    };

    [[nodiscard]] constexpr float pt(int point_size) noexcept {
        return static_cast<float>(point_size) * 96.0f / 72.0f;
    }

    enum class font_render_mode : dh::u8 { natural, aliased };

    enum class font_weight : dh::u16 {
        thin = 100,
        extra_light = 200,
        light = 300,
        normal = 400,
        medium = 500,
        semi_bold = 600,
        bold = 700,
        extra_bold = 800,
        black = 900,
    };
    inline constexpr dh::u16 k_min_font_weight = 1;
    inline constexpr dh::u16 k_max_font_weight = 999;
    [[nodiscard]] constexpr bool valid_font_weight(font_weight weight) noexcept {
        const auto value = static_cast<dh::u16>(weight);
        return value >= k_min_font_weight && value <= k_max_font_weight;
    }
    enum class font_slant : dh::u8 { normal, italic };
    enum class text_flow : dh::u8 { trim, wrap };
    enum class text_alignment : dh::u8 { left, center, right };

    struct text_layout_options final {
    public:
        constexpr text_layout_options(text_flow flow_value = text_flow::wrap, text_alignment alignment_value = text_alignment::left) noexcept : flow(flow_value), alignment(alignment_value) {}
        ~text_layout_options() = default;
        text_layout_options(const text_layout_options&) = default;
        text_layout_options& operator=(const text_layout_options&) = default;
        text_layout_options(text_layout_options&&) noexcept = default;
        text_layout_options& operator=(text_layout_options&&) noexcept = default;
        text_flow flow;
        text_alignment alignment;
    };

    struct font_metrics final {
    public:
        font_metrics() noexcept = default;
        ~font_metrics() = default;
        font_metrics(const font_metrics&) = default;
        font_metrics& operator=(const font_metrics&) = default;
        font_metrics(font_metrics&&) noexcept = default;
        font_metrics& operator=(font_metrics&&) noexcept = default;
        float ascent{}, descent{}, line_gap{}, line_height{};
        [[nodiscard]] font_metrics scaled(float factor) const noexcept {
            auto result = *this;
            result.ascent *= factor;
            result.descent *= factor;
            result.line_gap *= factor;
            result.line_height *= factor;
            return result;
        }
    };

    struct text_metrics final {
    public:
        text_metrics() noexcept = default;
        ~text_metrics() = default;
        text_metrics(const text_metrics&) = default;
        text_metrics& operator=(const text_metrics&) = default;
        text_metrics(text_metrics&&) noexcept = default;
        text_metrics& operator=(text_metrics&&) noexcept = default;
        float width{}, height{}, left{}, top{};
        float ink_left{}, ink_top{}, ink_right{}, ink_bottom{};
        dh::u32 line_count{};
        [[nodiscard]] text_metrics scaled(float factor) const noexcept {
            auto result = *this;
            result.width *= factor;
            result.height *= factor;
            result.left *= factor;
            result.top *= factor;
            result.ink_left *= factor;
            result.ink_top *= factor;
            result.ink_right *= factor;
            result.ink_bottom *= factor;
            return result;
        }
    };

    struct text_size final {
    public:
        constexpr text_size(float width_in, float height_in) noexcept : width(width_in), height(height_in) {}
        ~text_size() = default;
        text_size(const text_size&) = default;
        text_size& operator=(const text_size&) = default;
        text_size(text_size&&) noexcept = default;
        text_size& operator=(text_size&&) noexcept = default;
        float width;
        float height;
    };

    struct text_caret final {
    public:
        constexpr text_caret(float x_value, float y_value) noexcept : x(x_value), y(y_value) {}
        ~text_caret() = default;
        text_caret(const text_caret&) = default;
        text_caret& operator=(const text_caret&) = default;
        text_caret(text_caret&&) noexcept = default;
        text_caret& operator=(text_caret&&) noexcept = default;
        float x;
        float y;
        [[nodiscard]] text_caret scaled(float factor) const noexcept { return {x * factor, y * factor}; }
    };

    struct font_config final {
        constexpr font_config(dh::wstring_view face, float em, font_weight weight_value = font_weight::normal, font_render_mode mode = font_render_mode::natural, font_slant slant_value = font_slant::normal) noexcept : face_name(face), em_size_px(em), weight(weight_value), render_mode(mode), slant(slant_value) {}

        ~font_config() = default;
        constexpr font_config(const font_config&) noexcept = default;
        constexpr font_config& operator=(const font_config&) noexcept = default;
        constexpr font_config(font_config&&) noexcept = default;
        constexpr font_config& operator=(font_config&&) noexcept = default;
        dh::wstring_view face_name;
        float em_size_px;
        font_weight weight = font_weight::normal;
        font_render_mode render_mode = font_render_mode::natural;
        font_slant slant = font_slant::normal;

        float letter_spacing_em = 0.0f;
    };

    inline constexpr float k_min_letter_spacing_em = -0.5f;
    inline constexpr float k_max_letter_spacing_em = 1.0f;

    struct text_glyph final {
        text_glyph() noexcept = default;
        ~text_glyph() = default;
        text_glyph(const text_glyph&) noexcept = default;
        text_glyph& operator=(const text_glyph&) noexcept = default;
        text_glyph(text_glyph&&) noexcept = default;
        text_glyph& operator=(text_glyph&&) noexcept = default;
        ID3D11ShaderResourceView* srv{};
        float left{};
        float top{};
        int width{};
        int height{};
        float u0{};
        float v0{};
        float u1{};
        float v1{};
    };

    class font_atlas final {
    public:
        font_atlas() noexcept;
        ~font_atlas();
        font_atlas(const font_atlas&) = delete;
        font_atlas& operator=(const font_atlas&) = delete;
        font_atlas(font_atlas&&) = delete;
        font_atlas& operator=(font_atlas&&) = delete;

        [[nodiscard]] dh::expected<void, atlas_failure> initialize(ID3D11Device* device, const font_config& config) noexcept;

        [[nodiscard]] dh::expected<void, atlas_failure> initialize_file(ID3D11Device* device, dh::span<const dh::u8> bytes, const font_config& config) noexcept;
        void shutdown() noexcept;
        void begin_frame() noexcept;

        [[nodiscard]] dh::expected<float, atlas_failure> measure(dh::string_view text) const noexcept;

        [[nodiscard]] dh::expected<text_caret, atlas_failure> trailing_caret(dh::string_view text) const noexcept;
        [[nodiscard]] dh::expected<text_size, atlas_failure> measure_layout(dh::string_view text, float width, text_layout_options options = {}) const noexcept;
        [[nodiscard]] dh::expected<text_metrics, atlas_failure> layout_metrics(dh::string_view text, float width, text_layout_options options = {}) const noexcept;
        [[nodiscard]] dh::expected<font_metrics, atlas_failure> metrics() const noexcept;
        // the returned span borrows the cache, consume it before requesting another layout.
        [[nodiscard]] dh::expected<dh::span<const text_glyph>, atlas_failure> prepare(dh::string_view text, float max_width, text_flow flow = text_flow::trim, text_alignment alignment = text_alignment::left) const noexcept;
        [[nodiscard]] int line_height() const noexcept;
        [[nodiscard]] int ascent() const noexcept;
        [[nodiscard]] bool is_initialized() const noexcept;
        [[nodiscard]] bool has_missing_glyphs() const noexcept;
        [[nodiscard]] ID3D11Device* device() const noexcept;

        static constexpr dh::size_t k_max_text_bytes = 4096;
        static constexpr dh::size_t k_max_cached_layouts = 512;
        static constexpr dh::size_t k_max_cached_glyph_placements = 65536;
        static constexpr dh::size_t k_max_atlas_pages = 4;
        static constexpr int k_atlas_page_size = 1024;
        static constexpr float k_max_em_size = 512.0f;
        static constexpr dh::size_t k_max_font_file_bytes = 4 * 1024 * 1024;

    private:
        class cache;
        dh::unique_ptr<cache> m_cache;
    };

}
