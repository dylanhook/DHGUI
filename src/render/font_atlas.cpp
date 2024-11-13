// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "font_atlas.h"

#include "core/dh/algorithm.h"
#include "core/dh/array.h"
#include "core/dh/hash_map.h"
#include "core/dh/math.h"
#include "core/dh/optional.h"
#include "core/dh/vector.h"

#include <dwrite_3.h>
#include <wrl/client.h>

namespace dh::render {

    dh::string_view to_string(atlas_error error) noexcept {
        switch (error) {
            case atlas_error::not_initialized:
                return "not_initialized";
            case atlas_error::allocation_failed:
                return "allocation_failed";
            case atlas_error::invalid_config:
                return "invalid_config";
            case atlas_error::create_factory_failed:
                return "create_factory_failed";
            case atlas_error::font_file_invalid: return "font_file_invalid";
            case atlas_error::font_loader_failed: return "font_loader_failed";
            case atlas_error::font_loader_register_failed: return "font_loader_register_failed";
            case atlas_error::font_file_create_failed: return "font_file_create_failed";
            case atlas_error::font_set_failed: return "font_set_failed";
            case atlas_error::get_font_collection_failed:
                return "get_font_collection_failed";
            case atlas_error::face_name_too_long:
                return "face_name_too_long";
            case atlas_error::font_not_found:
                return "font_not_found";
            case atlas_error::create_face_failed:
                return "create_face_failed";
            case atlas_error::face_reference_failed:
                return "face_reference_failed";
            case atlas_error::compute_origins_failed:
                return "compute_origins_failed";
            case atlas_error::create_format_failed:
                return "create_format_failed";
            case atlas_error::configure_format_failed:
                return "configure_format_failed";
            case atlas_error::create_ellipsis_failed:
                return "create_ellipsis_failed";
            case atlas_error::text_too_long:
                return "text_too_long";
            case atlas_error::invalid_utf8:
                return "invalid_utf8";
            case atlas_error::invalid_width:
                return "invalid_width";
            case atlas_error::classify_direction_failed:
                return "classify_direction_failed";
            case atlas_error::create_layout_failed:
                return "create_layout_failed";
            case atlas_error::configure_layout_failed:
                return "configure_layout_failed";
            case atlas_error::layout_metrics_failed:
                return "layout_metrics_failed";
            case atlas_error::draw_layout_failed:
                return "draw_layout_failed";
            case atlas_error::unsupported_decoration:
                return "unsupported_decoration";
            case atlas_error::missing_glyph:
                return "missing_glyph";
            case atlas_error::create_glyph_run_analysis_failed:
                return "create_glyph_run_analysis_failed";
            case atlas_error::glyph_texture_bounds_failed:
                return "glyph_texture_bounds_failed";
            case atlas_error::create_alpha_texture_failed:
                return "create_alpha_texture_failed";
            case atlas_error::raster_too_large:
                return "raster_too_large";
            case atlas_error::cache_full:
                return "cache_full";
            case atlas_error::submission_full:
                return "submission_full";
            case atlas_error::device_mismatch:
                return "device_mismatch";
            case atlas_error::create_texture_failed:
                return "create_texture_failed";
            case atlas_error::create_srv_failed:
                return "create_srv_failed";
            case atlas_error::hit_test_failed:
                return "hit_test_failed";
        }
        return "unknown";
    }

    namespace {

        template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>;

        constexpr dh::size_t k_max_faces = 128;
        constexpr dh::size_t k_max_glyphs_per_layout = 16384;
        constexpr dh::size_t k_max_glyphs_per_page = 8192;
        constexpr dh::size_t k_no_slot = font_atlas::k_max_cached_layouts;
        constexpr float k_layout_extent = 1048576.0f;
        constexpr dh::size_t k_face_name_capacity = 128;
        constexpr int k_padding = 1;
        static_assert(k_max_glyphs_per_layout <= font_atlas::k_max_cached_glyph_placements);
        static_assert(font_atlas::k_max_atlas_pages <= sizeof(dh::u32) * 8);

        struct layout_key final {
            layout_key(dh::string_view value, float limit, text_flow mode, text_alignment alignment_value) noexcept : text(value), width(limit), flow(mode), alignment(alignment_value) {}

            ~layout_key() = default;
            layout_key(const layout_key&) noexcept = default;
            layout_key& operator=(const layout_key&) noexcept = default;
            layout_key(layout_key&&) noexcept = default;
            layout_key& operator=(layout_key&&) noexcept = default;
            dh::string_view text;
            float width{};
            text_flow flow{};
            text_alignment alignment{};

            [[nodiscard]] bool operator==(const layout_key& other) const noexcept {
                return width == other.width && flow == other.flow && alignment == other.alignment && text == other.text;
            }
        };

        struct layout_hash final {
            layout_hash() noexcept = default;
            ~layout_hash() = default;
            layout_hash(const layout_hash&) noexcept = default;
            layout_hash& operator=(const layout_hash&) noexcept = default;
            layout_hash(layout_hash&&) noexcept = default;
            layout_hash& operator=(layout_hash&&) noexcept = default;

            [[nodiscard]] dh::u64 operator()(const layout_key& key) const noexcept {
                dh::fnv1a hash;
                hash.combine(dh::hash<dh::string_view>{}(key.text));
                hash.combine(dh::hash<float>{}(key.width));
                hash.combine(static_cast<dh::u64>(key.flow));
                hash.combine(static_cast<dh::u64>(key.alignment));
                return hash.value();
            }
        };

        struct glyph_key final {
            glyph_key() noexcept = default;

            ~glyph_key() = default;
            glyph_key(const glyph_key&) noexcept = default;
            glyph_key& operator=(const glyph_key&) noexcept = default;
            glyph_key(glyph_key&&) noexcept = default;
            glyph_key& operator=(glyph_key&&) noexcept = default;
            dh::u32 face{};
            dh::u16 index{};
            bool sideways{};
            DWRITE_MEASURING_MODE measuring{};
            float em{};
            float phase_x{};
            float phase_y{};
            [[nodiscard]] bool operator==(const glyph_key&) const noexcept = default;
        };

        struct glyph_hash final {
            glyph_hash() noexcept = default;
            ~glyph_hash() = default;
            glyph_hash(const glyph_hash&) noexcept = default;
            glyph_hash& operator=(const glyph_hash&) noexcept = default;
            glyph_hash(glyph_hash&&) noexcept = default;
            glyph_hash& operator=(glyph_hash&&) noexcept = default;

            [[nodiscard]] dh::u64 operator()(const glyph_key& key) const noexcept {
                dh::fnv1a hash;
                hash.combine(key.face);
                hash.combine(key.index);
                hash.combine(key.sideways);
                hash.combine(static_cast<dh::u64>(key.measuring));
                hash.combine(dh::hash<float>{}(key.em));
                hash.combine(dh::hash<float>{}(key.phase_x));
                hash.combine(dh::hash<float>{}(key.phase_y));
                return hash.value();
            }
        };

        struct shaped_glyph final {
            shaped_glyph(glyph_key identity, float x, float y) noexcept : key(identity), origin_x(x), origin_y(y) {}

            ~shaped_glyph() = default;
            shaped_glyph(const shaped_glyph&) noexcept = default;
            shaped_glyph& operator=(const shaped_glyph&) noexcept = default;
            shaped_glyph(shaped_glyph&&) noexcept = default;
            shaped_glyph& operator=(shaped_glyph&&) noexcept = default;
            glyph_key key;
            float origin_x{};
            float origin_y{};
        };

        struct face_entry final {
            face_entry(IDWriteFontFace* value, com_ptr<IDWriteFontFaceReference> identity) noexcept : face(value), reference(dh::move(identity)) {}

            ~face_entry() = default;
            face_entry(const face_entry&) = delete;
            face_entry& operator=(const face_entry&) = delete;
            face_entry(face_entry&&) noexcept = default;
            face_entry& operator=(face_entry&&) noexcept = default;
            com_ptr<IDWriteFontFace> face;
            com_ptr<IDWriteFontFaceReference> reference;
        };

        struct layout_entry final {
            layout_entry() noexcept = default;
            ~layout_entry() = default;
            layout_entry(const layout_entry&) = delete;
            layout_entry& operator=(const layout_entry&) = delete;
            layout_entry(layout_entry&&) noexcept = default;
            layout_entry& operator=(layout_entry&&) noexcept = default;

            dh::string text;
            float limit{};
            float width{};
            float height{};
            text_alignment alignment{};
            text_metrics metrics{};
            text_flow flow{text_flow::trim};
            dh::u64 last_used{};
            dh::u64 generation{};
            dh::u32 page_mask{};
            com_ptr<IDWriteTextLayout> layout;
            dh::vector<shaped_glyph> shaped;
            dh::vector<text_glyph> prepared;
            dh::optional<atlas_failure> failure;
            dh::optional<text_caret> trailing;
            dh::optional<atlas_failure> caret_failure;
        };

        struct atlas_page final {
            atlas_page() noexcept = default;
            ~atlas_page() = default;
            atlas_page(const atlas_page&) = delete;
            atlas_page& operator=(const atlas_page&) = delete;
            atlas_page(atlas_page&&) noexcept = default;
            atlas_page& operator=(atlas_page&&) noexcept = default;

            com_ptr<ID3D11Texture2D> texture;
            com_ptr<ID3D11ShaderResourceView> srv;
            dh::hash_map<glyph_key, text_glyph, glyph_hash> glyphs;
            int pen_x{};
            int pen_y{};
            int row_height{};
            dh::u64 last_used{};
        };

    }

    class font_atlas::cache final {
    public:
        cache() noexcept = default;
        ~cache() {
            m_layouts.clear();
            m_faces.clear();
            m_ellipsis.Reset();
            m_format.Reset();
            if (m_font_loader && FAILED(m_factory->UnregisterFontFileLoader(m_font_loader.Get())))
                dh::fail_fast("font file loader did not unregister", FAST_FAIL_INVALID_ARG);
        }
        cache(const cache&) = delete;
        cache& operator=(const cache&) = delete;
        cache(cache&&) = delete;
        cache& operator=(cache&&) = delete;

        [[nodiscard]] dh::expected<void, atlas_failure> initialize(ID3D11Device* device, const font_config& config, dh::span<const dh::u8> bytes) noexcept;
        [[nodiscard]] dh::expected<layout_entry*, atlas_failure> find_layout(dh::string_view text, float width, text_flow flow, text_alignment alignment) noexcept;
        [[nodiscard]] dh::expected<dh::span<const text_glyph>, atlas_failure> prepare(layout_entry& entry) noexcept;
        void begin_frame() noexcept;

        [[nodiscard]] int line_height() const noexcept { return m_line_height; }

        [[nodiscard]] int ascent() const noexcept { return m_ascent; }
        [[nodiscard]] font_metrics metrics() const noexcept { return m_metrics; }

        [[nodiscard]] bool has_missing_glyphs() const noexcept { return m_missing_glyphs; }

        [[nodiscard]] bool has_device() const noexcept { return m_device != nullptr; }
        [[nodiscard]] ID3D11Device* device() const noexcept { return m_device.Get(); }

    private:
        class collector final : public IDWriteTextRenderer {
        public:
            collector(cache& owner, layout_entry& entry) noexcept : m_owner(owner), m_entry(entry) {}

            ~collector() = default;
            collector(const collector&) = delete;
            collector& operator=(const collector&) = delete;
            collector(collector&&) = delete;
            collector& operator=(collector&&) = delete;

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override {
                if (!object) return E_POINTER;
                *object = nullptr;
                if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteTextRenderer))
                    *object = static_cast<IDWriteTextRenderer*>(this);
                else if (iid == __uuidof(IDWritePixelSnapping))
                    *object = static_cast<IDWritePixelSnapping*>(this);
                else
                    return E_NOINTERFACE;
                AddRef();
                return S_OK;
            }

            ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++m_references; }

            ULONG STDMETHODCALLTYPE Release() noexcept override { return --m_references; }

            HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) noexcept override {
                *disabled = TRUE;
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* matrix) noexcept override {
                *matrix = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* scale) noexcept override {
                *scale = 1.0f;
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE measuring, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) noexcept override;

            HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) noexcept override {
                return refuse({atlas_error::unsupported_decoration, E_NOTIMPL});
            }

            HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) noexcept override {
                return refuse({atlas_error::unsupported_decoration, E_NOTIMPL});
            }

            HRESULT STDMETHODCALLTYPE DrawInlineObject(void* context, FLOAT x, FLOAT y, IDWriteInlineObject* object, BOOL sideways, BOOL right_to_left, IUnknown* effect) noexcept override {
                return object->Draw(context, this, x, y, sideways, right_to_left, effect);
            }

        private:
            [[nodiscard]] HRESULT refuse(atlas_failure failure) noexcept {
                m_entry.failure = failure;
                return FAILED(failure.hr) ? failure.hr : E_FAIL;
            }

            cache& m_owner;
            layout_entry& m_entry;
            ULONG m_references{1};
        };

        [[nodiscard]] dh::expected<dh::u32, atlas_failure> resolve_face(IDWriteFontFace* face) noexcept;
        [[nodiscard]] dh::expected<void, atlas_failure> shape(layout_entry& entry, dh::wstring_view text) noexcept;
        [[nodiscard]] dh::expected<text_glyph, atlas_failure> rasterize(const glyph_key& key, dh::u32& page_mask) noexcept;
        [[nodiscard]] dh::expected<void, atlas_failure> create_page(atlas_page& page) noexcept;
        void evict_layout(dh::size_t index) noexcept;
        void evict_oldest_layout() noexcept;
        void recycle_page(dh::size_t index) noexcept;

        com_ptr<ID3D11Device> m_device;
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<IDWriteFactory4> m_factory;
        com_ptr<IDWriteInMemoryFontFileLoader> m_font_loader;
        com_ptr<IDWriteTextFormat> m_format;
        com_ptr<IDWriteInlineObject> m_ellipsis;
        font_render_mode m_mode{};

        float m_letter_spacing_px{0.0f};
        int m_line_height{};
        int m_ascent{};
        font_metrics m_metrics{};
        dh::u64 m_clock{1};
        dh::u64 m_generation{1};
        dh::size_t m_placement_count{};
        bool m_page_pressure{};
        bool m_face_pressure{};
        bool m_missing_glyphs{};
        dh::array<atlas_page, k_max_atlas_pages> m_pages;
        dh::vector<face_entry> m_faces;
        dh::vector<layout_entry> m_layouts;
        dh::vector<dh::size_t> m_free_slots;
        dh::hash_map<layout_key, dh::size_t, layout_hash> m_layout_index;
        dh::vector<D2D1_POINT_2F> m_origins;
        dh::vector<dh::u8> m_alpha;
        dh::vector<dh::u8> m_upload;
    };

    dh::expected<void, atlas_failure> font_atlas::cache::initialize(ID3D11Device* device, const font_config& config, dh::span<const dh::u8> bytes) noexcept {
        if ((config.face_name.empty() && bytes.empty()) || !dh::isfinite(config.em_size_px) || config.em_size_px <= 0.0f || config.em_size_px > k_max_em_size || !valid_font_weight(config.weight) || !dh::isfinite(config.letter_spacing_em) || config.letter_spacing_em < k_min_letter_spacing_em || config.letter_spacing_em > k_max_letter_spacing_em || (config.render_mode != font_render_mode::natural && config.render_mode != font_render_mode::aliased) || (config.slant != font_slant::normal && config.slant != font_slant::italic))
            return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};
        if (config.face_name.size() >= k_face_name_capacity) return dh::unexpected{atlas_failure{atlas_error::face_name_too_long, E_INVALIDARG}};
        for (const wchar_t character : config.face_name)
            if (character == L'\0') return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_INVALIDARG}};

        IUnknown* raw_factory = nullptr;
        HRESULT hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory4), &raw_factory);
        m_factory.Attach(static_cast<IDWriteFactory4*>(raw_factory));
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_factory_failed, hr}};

        com_ptr<IDWriteFontCollection> collection;
        dh::array<wchar_t, k_face_name_capacity> name{};
        UINT32 index{};
        if (bytes.empty()) {
            hr = m_factory->GetSystemFontCollection(collection.GetAddressOf(), FALSE);
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::get_font_collection_failed, hr}};
            dh::memcpy(name.data(), config.face_name.data(), config.face_name.size() * sizeof(wchar_t));
            BOOL exists{};
            hr = collection->FindFamilyName(name.data(), &index, &exists);
            if (FAILED(hr) || !exists) return dh::unexpected{atlas_failure{atlas_error::font_not_found, hr}};
        } else {
            com_ptr<IDWriteFactory5> file_factory;
            hr = m_factory.As(&file_factory);
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_loader_failed, hr}};
            com_ptr<IDWriteInMemoryFontFileLoader> loader;
            hr = file_factory->CreateInMemoryFontFileLoader(loader.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_loader_failed, hr}};
            hr = m_factory->RegisterFontFileLoader(loader.Get());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_loader_register_failed, hr}};
            m_font_loader = dh::move(loader);
            com_ptr<IDWriteFontFile> file;

            hr = m_font_loader->CreateInMemoryFontFileReference(m_factory.Get(), bytes.data(), static_cast<UINT32>(bytes.size()), nullptr, file.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_file_create_failed, hr}};
            BOOL supported{};
            DWRITE_FONT_FILE_TYPE file_type{};
            DWRITE_FONT_FACE_TYPE face_type{};
            UINT32 faces{};
            hr = file->Analyze(&supported, &file_type, &face_type, &faces);
            if (FAILED(hr) || !supported || faces != 1 || (file_type != DWRITE_FONT_FILE_TYPE_TRUETYPE && file_type != DWRITE_FONT_FILE_TYPE_CFF))
                return dh::unexpected{atlas_failure{atlas_error::font_file_invalid, FAILED(hr) ? hr : E_INVALIDARG}};
            com_ptr<IDWriteFontSetBuilder1> builder;
            hr = file_factory->CreateFontSetBuilder(builder.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_set_failed, hr}};
            hr = builder->AddFontFile(file.Get());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_set_failed, hr}};
            com_ptr<IDWriteFontSet> set;
            hr = builder->CreateFontSet(set.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_set_failed, hr}};
            com_ptr<IDWriteFontCollection1> custom;
            hr = m_factory->CreateFontCollectionFromFontSet(set.Get(), custom.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::get_font_collection_failed, hr}};
            collection = custom;
            if (collection->GetFontFamilyCount() != 1)
                return dh::unexpected{atlas_failure{atlas_error::font_file_invalid, E_INVALIDARG}};
        }
        com_ptr<IDWriteFontFamily> family;
        hr = collection->GetFontFamily(index, family.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_face_failed, hr}};
        if (!bytes.empty()) {
            com_ptr<IDWriteLocalizedStrings> names;
            hr = family->GetFamilyNames(names.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_file_invalid, hr}};
            UINT32 length{};
            hr = names->GetStringLength(0, &length);
            if (FAILED(hr) || length >= name.size())
                return dh::unexpected{atlas_failure{atlas_error::face_name_too_long, FAILED(hr) ? hr : E_INVALIDARG}};
            hr = names->GetString(0, name.data(), static_cast<UINT32>(name.size()));
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::font_file_invalid, hr}};
        }
        const auto weight = static_cast<DWRITE_FONT_WEIGHT>(static_cast<dh::u16>(config.weight));
        const auto slant = config.slant == font_slant::italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
        com_ptr<IDWriteFont> font;
        hr = family->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, slant, font.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_face_failed, hr}};
        com_ptr<IDWriteFontFace> face;
        hr = font->CreateFontFace(face.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_face_failed, hr}};

        DWRITE_FONT_METRICS metrics{};
        if (config.render_mode == font_render_mode::aliased) {
            hr = face->GetGdiCompatibleMetrics(config.em_size_px, 1.0f, nullptr, &metrics);
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_face_failed, hr}};
        } else {
            face->GetMetrics(&metrics);
        }
        if (!metrics.designUnitsPerEm) return dh::unexpected{atlas_failure{atlas_error::invalid_config, E_FAIL}};
        const float scale = config.em_size_px / static_cast<float>(metrics.designUnitsPerEm);
        m_ascent = static_cast<int>(dh::ceil(static_cast<float>(metrics.ascent) * scale));
        m_line_height = static_cast<int>(dh::ceil(static_cast<float>(metrics.ascent + metrics.descent + metrics.lineGap) * scale));
        m_metrics.ascent = static_cast<float>(m_ascent);
        m_metrics.descent = static_cast<float>(metrics.descent) * scale;
        m_metrics.line_gap = static_cast<float>(metrics.lineGap) * scale;
        m_metrics.line_height = static_cast<float>(m_line_height);

        dh::array<wchar_t, LOCALE_NAME_MAX_LENGTH> locale{};
        if (!::GetUserDefaultLocaleName(locale.data(), static_cast<int>(locale.size())))
            return dh::unexpected{atlas_failure{atlas_error::create_format_failed, HRESULT_FROM_WIN32(::GetLastError())}};
        hr = m_factory->CreateTextFormat(name.data(), collection.Get(), weight, slant, DWRITE_FONT_STRETCH_NORMAL, config.em_size_px, locale.data(), m_format.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_format_failed, hr}};
        hr = m_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_format_failed, hr}};

        hr = m_format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, static_cast<float>(m_line_height), static_cast<float>(m_ascent));
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_format_failed, hr}};
        hr = m_factory->CreateEllipsisTrimmingSign(m_format.Get(), m_ellipsis.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_ellipsis_failed, hr}};

        m_device = device;
        if (device != nullptr) device->GetImmediateContext(m_context.GetAddressOf());
        m_mode = config.render_mode;
        m_letter_spacing_px = config.letter_spacing_em * config.em_size_px;
        m_faces.reserve(k_max_faces);
        m_layouts.resize(k_max_cached_layouts);
        m_free_slots.reserve(k_max_cached_layouts);
        m_layout_index.reserve(k_max_cached_layouts * 2);
        for (dh::size_t slot = k_max_cached_layouts; slot > 0; --slot)
            m_free_slots.push_back(slot - 1);
        return {};
    }

    dh::expected<dh::u32, atlas_failure> font_atlas::cache::resolve_face(IDWriteFontFace* face) noexcept {
        for (dh::size_t i = 0; i < m_faces.size(); ++i)
            if (m_faces[i].face.Get() == face) return static_cast<dh::u32>(i);

        com_ptr<IDWriteFontFace3> face3;
        HRESULT hr = face->QueryInterface(IID_PPV_ARGS(face3.GetAddressOf()));
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::face_reference_failed, hr}};
        com_ptr<IDWriteFontFaceReference> reference;
        hr = face3->GetFontFaceReference(reference.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::face_reference_failed, hr}};

        for (dh::size_t i = 0; i < m_faces.size(); ++i)
            if (m_faces[i].reference->Equals(reference.Get())) return static_cast<dh::u32>(i);
        if (m_faces.size() == k_max_faces) {
            m_face_pressure = true;
            return dh::unexpected{atlas_failure{atlas_error::cache_full, S_OK}};
        }
        m_faces.push_back({face, dh::move(reference)});
        return static_cast<dh::u32>(m_faces.size() - 1);
    }

    HRESULT font_atlas::cache::collector::DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE measuring, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) noexcept {
        if (run->glyphCount > k_max_glyphs_per_layout - m_entry.shaped.size()) return refuse({atlas_error::text_too_long, E_INVALIDARG});
        const auto face = m_owner.resolve_face(run->fontFace);
        if (!face) return refuse(face.error());
        m_owner.m_origins.resize(run->glyphCount);
        const HRESULT hr = m_owner.m_factory->ComputeGlyphOrigins(run, measuring, {x, y}, nullptr, m_owner.m_origins.data());
        if (FAILED(hr)) return refuse({atlas_error::compute_origins_failed, hr});

        for (UINT32 i = 0; i < run->glyphCount; ++i) {
            if (run->glyphIndices[i] == 0) m_owner.m_missing_glyphs = true;
            const auto origin = m_owner.m_origins[i];
            if (!dh::isfinite(origin.x) || !dh::isfinite(origin.y) || dh::fabs(origin.x) > k_layout_extent || dh::fabs(origin.y) > k_layout_extent)
                return refuse({atlas_error::raster_too_large, E_INVALIDARG});
            const float integer_x = dh::floor(origin.x);
            const float integer_y = dh::floor(origin.y);
            glyph_key key;
            key.face = *face;
            key.index = run->glyphIndices[i];
            key.sideways = run->isSideways != FALSE;
            key.measuring = measuring;
            key.em = run->fontEmSize;
            key.phase_x = origin.x - integer_x;
            key.phase_y = origin.y - integer_y;
            m_entry.shaped.push_back({key, integer_x, integer_y});
        }
        return S_OK;
    }

    dh::expected<void, atlas_failure> font_atlas::cache::shape(layout_entry& entry, dh::wstring_view text) noexcept {
        const float width = entry.limit > 0.0f ? entry.limit : k_layout_extent;
        HRESULT hr = m_mode == font_render_mode::aliased ?
            m_factory->CreateGdiCompatibleTextLayout(text.data(), static_cast<UINT32>(text.size()), m_format.Get(), width, k_layout_extent, 1.0f, nullptr, FALSE, entry.layout.GetAddressOf()) : m_factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), m_format.Get(), width, k_layout_extent, entry.layout.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_layout_failed, hr}};

        dh::array<WORD, k_max_text_bytes> directions{};
        if (!text.empty() && !::GetStringTypeW(CT_CTYPE2, text.data(), static_cast<int>(text.size()), directions.data()))
            return dh::unexpected{atlas_failure{atlas_error::classify_direction_failed, HRESULT_FROM_WIN32(::GetLastError())}};
        bool right_to_left = false;
        for (dh::size_t i = 0; i < text.size(); ++i) {
            if (directions[i] == C2_RIGHTTOLEFT) {
                right_to_left = true;
                break;
            }
            if (directions[i] == C2_LEFTTORIGHT) break;
        }
        hr = entry.layout->SetReadingDirection(right_to_left ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};

        const auto alignment = entry.alignment == text_alignment::center ? DWRITE_TEXT_ALIGNMENT_CENTER :
            ((entry.alignment == text_alignment::right) != right_to_left ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
        hr = entry.layout->SetTextAlignment(alignment);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
        if (m_letter_spacing_px != 0.0f && !text.empty()) {
            com_ptr<IDWriteTextLayout1> spaced;
            hr = entry.layout.As(&spaced);
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
            hr = spaced->SetCharacterSpacing(0.0f, m_letter_spacing_px, 0.0f, DWRITE_TEXT_RANGE{0, static_cast<UINT32>(text.size())});
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
        }
        if (entry.flow == text_flow::wrap) {
            hr = entry.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
        } else if (entry.limit > 0.0f) {
            const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            hr = entry.layout->SetTrimming(&trimming, m_ellipsis.Get());
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
        }
        DWRITE_TEXT_METRICS metrics{};
        hr = entry.layout->GetMetrics(&metrics);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::layout_metrics_failed, hr}};
        entry.width = metrics.widthIncludingTrailingWhitespace;
        entry.height = metrics.height;
        if (entry.limit == 0.0f) {
            hr = entry.layout->SetMaxWidth(entry.width);
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
        }
        hr = entry.layout->SetMaxHeight(metrics.height);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::configure_layout_failed, hr}};
        hr = entry.layout->GetMetrics(&metrics);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::layout_metrics_failed, hr}};
        DWRITE_OVERHANG_METRICS ink{};
        hr = entry.layout->GetOverhangMetrics(&ink);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::layout_metrics_failed, hr}};
        entry.metrics.width = metrics.widthIncludingTrailingWhitespace;
        entry.metrics.height = metrics.height;
        entry.metrics.left = metrics.left;
        entry.metrics.top = metrics.top;
        entry.metrics.line_count = metrics.lineCount;
        entry.metrics.ink_left = -ink.left;
        entry.metrics.ink_top = -ink.top;
        entry.metrics.ink_right = entry.layout->GetMaxWidth() + ink.right;
        entry.metrics.ink_bottom = entry.layout->GetMaxHeight() + ink.bottom;
        if (text.empty()) entry.trailing.emplace(entry.metrics.left, entry.metrics.top);
        else {
            FLOAT x = 0.0f;
            FLOAT y = 0.0f;
            DWRITE_HIT_TEST_METRICS hit{};
            // let DirectWrite find the caret, one character can span multiple utf16 units.
            const HRESULT hit_result = entry.layout->HitTestTextPosition(static_cast<UINT32>(text.size() - 1), TRUE, &x, &y, &hit);
            if (FAILED(hit_result)) entry.caret_failure.emplace(atlas_error::hit_test_failed, hit_result);
            else entry.trailing.emplace(x, y);
        }
        collector renderer{*this, entry};
        hr = entry.layout->Draw(nullptr, &renderer, 0.0f, 0.0f);
        if (entry.failure) return dh::unexpected{*entry.failure};
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::draw_layout_failed, hr}};
        return {};
    }

    dh::expected<layout_entry*, atlas_failure> font_atlas::cache::find_layout(dh::string_view text, float width, text_flow flow, text_alignment alignment) noexcept {
        if (text.size() > k_max_text_bytes) return dh::unexpected{atlas_failure{atlas_error::text_too_long, E_INVALIDARG}};
        if (!dh::isfinite(width) || width < 0.0f || width > k_layout_extent) return dh::unexpected{atlas_failure{atlas_error::invalid_width, E_INVALIDARG}};
        if ((flow != text_flow::trim && flow != text_flow::wrap) || (flow == text_flow::wrap && width <= 0.0f) || (alignment != text_alignment::left && alignment != text_alignment::center && alignment != text_alignment::right))
            return dh::unexpected{atlas_failure{atlas_error::invalid_width, E_INVALIDARG}};
        const layout_key key{text, width, flow, alignment};
        for (auto& entry : m_layouts) {
            if (entry.last_used && layout_key{entry.text, entry.limit, entry.flow, entry.alignment} == key) {
                entry.last_used = ++m_clock;
                return &entry;
            }
        }
        dh::array<wchar_t, k_max_text_bytes> wide{};
        int length = 0;
        if (!text.empty()) {
            length =
                ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), static_cast<int>(wide.size()));
            if (!length) return dh::unexpected{atlas_failure{atlas_error::invalid_utf8, HRESULT_FROM_WIN32(::GetLastError())}};
        }

        layout_entry staged;
        staged.text = dh::string{text};
        staged.limit = width;
        staged.flow = flow;
        staged.alignment = alignment;
        staged.last_used = ++m_clock;
        const auto result = shape(staged, {wide.data(), static_cast<dh::size_t>(length)});
        if (!result) return dh::unexpected{result.error()};

        if (m_free_slots.empty()) evict_oldest_layout();
        while (staged.shaped.size() > k_max_cached_glyph_placements - m_placement_count)
            evict_oldest_layout();
        const dh::size_t slot = m_free_slots.back();
        m_free_slots.pop_back();
        m_placement_count += staged.shaped.size();
        m_layouts[slot] = dh::move(staged);
        auto& entry = m_layouts[slot];
        m_layout_index.emplace(layout_key{entry.text, entry.limit, entry.flow, entry.alignment}, slot);
        return &entry;
    }

    void font_atlas::cache::evict_layout(dh::size_t index) noexcept {
        auto& entry = m_layouts[index];
        m_layout_index.erase(layout_key{entry.text, entry.limit, entry.flow, entry.alignment});
        m_placement_count -= entry.shaped.size();
        entry = layout_entry{};
        m_free_slots.push_back(index);
    }

    void font_atlas::cache::evict_oldest_layout() noexcept {
        dh::size_t oldest = k_no_slot;
        for (dh::size_t i = 0; i < m_layouts.size(); ++i) {
            if (!m_layouts[i].last_used) continue;
            if (oldest == k_no_slot || m_layouts[i].last_used < m_layouts[oldest].last_used) oldest = i;
        }
        if (oldest != k_no_slot) evict_layout(oldest);
    }

    void font_atlas::cache::recycle_page(dh::size_t index) noexcept {
        auto& page = m_pages[index];
        page.glyphs.clear();
        page.pen_x = page.pen_y = page.row_height = 0;
        page.last_used = ++m_clock;
        // recycling changes glyph uvs, cached placements need rebuilding before reuse.
        ++m_generation;
    }

    // recycle between frames, queued text still needs the old atlas pixels until then.
    void font_atlas::cache::begin_frame() noexcept {
        if (m_face_pressure) {
            m_layout_index.clear();
            m_free_slots.clear();
            for (dh::size_t i = 0; i < m_layouts.size(); ++i) {
                m_layouts[i] = layout_entry{};
                m_free_slots.push_back(i);
            }
            m_placement_count = 0;
            for (dh::size_t i = 0; i < m_pages.size(); ++i)
                recycle_page(i);
            m_faces.clear();
            m_face_pressure = false;
            m_page_pressure = false;
        }
        if (m_page_pressure) {
            dh::size_t oldest = 0;
            for (dh::size_t i = 1; i < m_pages.size(); ++i)
                if (m_pages[i].last_used < m_pages[oldest].last_used) oldest = i;
            recycle_page(oldest);
            m_page_pressure = false;
        }
    }

    dh::expected<void, atlas_failure> font_atlas::cache::create_page(atlas_page& page) noexcept {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = k_atlas_page_size;
        desc.Height = k_atlas_page_size;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        com_ptr<ID3D11Texture2D> texture;
        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_texture_failed, hr}};
        com_ptr<ID3D11ShaderResourceView> srv;
        hr = m_device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_srv_failed, hr}};
        page.texture = dh::move(texture);
        page.srv = dh::move(srv);
        return {};
    }

    dh::expected<text_glyph, atlas_failure> font_atlas::cache::rasterize(const glyph_key& key, dh::u32& page_mask) noexcept {
        for (dh::size_t i = 0; i < m_pages.size(); ++i) {
            auto& page = m_pages[i];
            if (const auto* glyph = page.glyphs.find(key)) {
                page.last_used = ++m_clock;
                page_mask |= dh::u32{1} << i;
                return *glyph;
            }
        }

        const FLOAT advance = 0.0f;
        const DWRITE_GLYPH_OFFSET offset{};

        const DWRITE_GLYPH_RUN run{m_faces[key.face].face.Get(), key.em, 1, &key.index, &advance, &offset, key.sideways, 0};
        com_ptr<IDWriteGlyphRunAnalysis> analysis;
        HRESULT hr = m_factory->CreateGlyphRunAnalysis(&run, nullptr, m_mode == font_render_mode::aliased ? DWRITE_RENDERING_MODE_ALIASED : DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, key.measuring, DWRITE_GRID_FIT_MODE_ENABLED, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, key.phase_x, key.phase_y, analysis.GetAddressOf());
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_glyph_run_analysis_failed, hr}};

        constexpr auto k_texture_type = DWRITE_TEXTURE_ALIASED_1x1;
        RECT bounds{};
        hr = analysis->GetAlphaTextureBounds(k_texture_type, &bounds);
        if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::glyph_texture_bounds_failed, hr}};
        const auto width = static_cast<dh::i64>(bounds.right) - bounds.left;
        const auto height = static_cast<dh::i64>(bounds.bottom) - bounds.top;
        if (width < 0 || height < 0 || width > k_atlas_page_size - 2 * k_padding || height > k_atlas_page_size - 2 * k_padding)
            return dh::unexpected{atlas_failure{atlas_error::raster_too_large, E_INVALIDARG}};
        text_glyph glyph{};
        glyph.left = static_cast<float>(bounds.left);
        glyph.top = static_cast<float>(bounds.top);
        glyph.width = static_cast<int>(width);
        glyph.height = static_cast<int>(height);
        const bool has_ink = width > 0 && height > 0;
        const int padded_width = has_ink ? glyph.width + 2 * k_padding : 0;
        const int padded_height = has_ink ? glyph.height + 2 * k_padding : 0;

        dh::size_t selected = k_max_atlas_pages;
        int column = 0;
        int row = 0;
        for (dh::size_t i = 0; i < m_pages.size(); ++i) {
            const auto& page = m_pages[i];
            if (page.glyphs.size() >= k_max_glyphs_per_page) continue;
            column = page.pen_x;
            row = page.pen_y;
            if (column + padded_width > k_atlas_page_size) {
                column = 0;
                row += page.row_height;
            }
            if (row + padded_height <= k_atlas_page_size) {
                selected = i;
                break;
            }
        }
        if (selected == k_max_atlas_pages) {
            m_page_pressure = true;
            return dh::unexpected{atlas_failure{atlas_error::cache_full, S_OK}};
        }
        auto& page = m_pages[selected];
        if (has_ink) {
            const dh::size_t pixels = static_cast<dh::size_t>(width * height);
            m_alpha.resize(pixels);
            hr = analysis->CreateAlphaTexture(k_texture_type, &bounds, m_alpha.data(), static_cast<UINT32>(pixels));
            if (FAILED(hr)) return dh::unexpected{atlas_failure{atlas_error::create_alpha_texture_failed, hr}};
            if (!page.texture) {
                const auto created = create_page(page);
                if (!created) return dh::unexpected{created.error()};
            }

            m_upload.assign(static_cast<dh::size_t>(padded_width * padded_height), dh::u8{0});
            for (int y = 0; y < glyph.height; ++y)
                dh::memcpy(m_upload.data() + (y + k_padding) * padded_width + k_padding, m_alpha.data() + y * glyph.width, static_cast<dh::size_t>(glyph.width));
            const D3D11_BOX box{
                static_cast<UINT>(column), static_cast<UINT>(row), 0, static_cast<UINT>(column + padded_width), static_cast<UINT>(row + padded_height), 1};
            m_context->UpdateSubresource(page.texture.Get(), 0, &box, m_upload.data(), static_cast<UINT>(padded_width), 0);
            if (row != page.pen_y) page.row_height = 0;
            page.pen_x = column + padded_width;
            page.pen_y = row;
            page.row_height = dh::max(page.row_height, padded_height);
            glyph.srv = page.srv.Get();
            constexpr float k_inverse_page_size = 1.0f / static_cast<float>(k_atlas_page_size);
            glyph.u0 = static_cast<float>(column + k_padding) * k_inverse_page_size;
            glyph.v0 = static_cast<float>(row + k_padding) * k_inverse_page_size;
            glyph.u1 = static_cast<float>(column + k_padding + glyph.width) * k_inverse_page_size;
            glyph.v1 = static_cast<float>(row + k_padding + glyph.height) * k_inverse_page_size;
        }
        page.glyphs.emplace(key, glyph);
        page.last_used = ++m_clock;
        page_mask |= dh::u32{1} << selected;
        return glyph;
    }

    dh::expected<dh::span<const text_glyph>, atlas_failure> font_atlas::cache::prepare(layout_entry& entry) noexcept {
        if (entry.generation == m_generation) {
            for (dh::size_t i = 0; i < m_pages.size(); ++i)
                if (entry.page_mask & (dh::u32{1} << i)) m_pages[i].last_used = ++m_clock;
            return dh::span<const text_glyph>{entry.prepared};
        }
        entry.prepared.clear();
        entry.page_mask = 0;
        entry.prepared.reserve(entry.shaped.size());
        for (const auto& shaped : entry.shaped) {
            auto glyph = rasterize(shaped.key, entry.page_mask);
            if (!glyph) {
                entry.prepared.clear();
                return dh::unexpected{glyph.error()};
            }
            if (!glyph->srv) continue;
            glyph->left += shaped.origin_x;
            glyph->top += shaped.origin_y;
            entry.prepared.push_back(*glyph);
        }
        entry.generation = m_generation;
        return dh::span<const text_glyph>{entry.prepared};
    }

    font_atlas::font_atlas() noexcept = default;
    font_atlas::~font_atlas() = default;

    dh::expected<void, atlas_failure> font_atlas::initialize(ID3D11Device* device, const font_config& config) noexcept {
        auto staged = dh::make_unique<cache>();
        if (!staged) return dh::unexpected{atlas_failure{atlas_error::allocation_failed, E_OUTOFMEMORY}};
        if (auto result = staged->initialize(device, config, {}); !result) return dh::unexpected{result.error()};
        m_cache = dh::move(staged);
        return {};
    }

    dh::expected<void, atlas_failure> font_atlas::initialize_file(ID3D11Device* device, dh::span<const dh::u8> bytes, const font_config& config) noexcept {
        if (bytes.empty() || bytes.size() > k_max_font_file_bytes)
            return dh::unexpected{atlas_failure{atlas_error::font_file_invalid, E_INVALIDARG}};
        auto staged = dh::make_unique<cache>();
        if (!staged) return dh::unexpected{atlas_failure{atlas_error::allocation_failed, E_OUTOFMEMORY}};
        font_config file_config = config;
        file_config.face_name = {};
        if (const auto result = staged->initialize(device, file_config, bytes); !result) return dh::unexpected{result.error()};
        m_cache = dh::move(staged);
        return {};
    }

    void font_atlas::shutdown() noexcept {
        m_cache.reset();
    }

    void font_atlas::begin_frame() noexcept {
        if (m_cache) m_cache->begin_frame();
    }

    int font_atlas::line_height() const noexcept {
        return m_cache ? m_cache->line_height() : 0;
    }

    int font_atlas::ascent() const noexcept {
        return m_cache ? m_cache->ascent() : 0;
    }

    bool font_atlas::is_initialized() const noexcept {
        return static_cast<bool>(m_cache);
    }

    bool font_atlas::has_missing_glyphs() const noexcept {
        return m_cache && m_cache->has_missing_glyphs();
    }

    dh::expected<float, atlas_failure> font_atlas::measure(dh::string_view text) const noexcept {
        if (!m_cache) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        const auto entry = m_cache->find_layout(text, 0.0f, text_flow::trim, text_alignment::left);
        if (!entry) return dh::unexpected{entry.error()};
        return (*entry)->width;
    }

    ID3D11Device* font_atlas::device() const noexcept {
        return m_cache ? m_cache->device() : nullptr;
    }

    dh::expected<text_caret, atlas_failure> font_atlas::trailing_caret(dh::string_view text) const noexcept {
        if (!m_cache) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        if (text.empty()) return text_caret{0.0f, 0.0f};
        const auto entry = m_cache->find_layout(text, 0.0f, text_flow::trim, text_alignment::left);
        if (!entry) return dh::unexpected{entry.error()};
        if ((*entry)->caret_failure) return dh::unexpected{*(*entry)->caret_failure};
        if (!(*entry)->trailing) return dh::unexpected{atlas_failure{atlas_error::hit_test_failed, E_UNEXPECTED}};
        return *(*entry)->trailing;
    }

    dh::expected<text_size, atlas_failure> font_atlas::measure_layout(dh::string_view text, float width, text_layout_options options) const noexcept {
        if (!m_cache) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        const auto entry = m_cache->find_layout(text, width, options.flow, options.alignment);
        if (!entry) return dh::unexpected{entry.error()};
        return text_size{(*entry)->width, (*entry)->height};
    }

    dh::expected<dh::span<const text_glyph>, atlas_failure> font_atlas::prepare(dh::string_view text, float max_width, text_flow flow, text_alignment alignment) const noexcept {
        if (!m_cache || !m_cache->has_device()) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        const auto entry = m_cache->find_layout(text, max_width, flow, alignment);
        if (!entry) return dh::unexpected{entry.error()};
        return m_cache->prepare(**entry);
    }

    dh::expected<text_metrics, atlas_failure> font_atlas::layout_metrics(dh::string_view text, float width, text_layout_options options) const noexcept {
        if (!m_cache) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        const auto entry = m_cache->find_layout(text, width, options.flow, options.alignment);
        if (!entry) return dh::unexpected{entry.error()};
        return (*entry)->metrics;
    }

    dh::expected<font_metrics, atlas_failure> font_atlas::metrics() const noexcept {
        if (!m_cache) return dh::unexpected{atlas_failure{atlas_error::not_initialized, S_OK}};
        return m_cache->metrics();
    }

}
