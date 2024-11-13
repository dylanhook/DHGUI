// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "render/path_geometry.h"
#include "core/dh/math.h"
#include "core/dh/algorithm.h"

namespace dh::render {
    namespace {
        using Microsoft::WRL::ComPtr;

        [[nodiscard]] bool finite(math::vector2 point) noexcept {
            return dh::isfinite(point.x) && dh::isfinite(point.y);
        }

        [[nodiscard]] D2D1_POINT_2F point(math::vector2 value) noexcept { return {value.x, value.y}; }

        [[nodiscard]] bool valid(dh::span<const path_command> commands, path_options options) noexcept {
            if (commands.empty() || commands.size() > path_geometry::k_max_commands || !(options.thickness > 0.0f) || !dh::isfinite(options.thickness) || !(options.miter_limit >= 1.0f) || !dh::isfinite(options.miter_limit) || (options.paint != path_paint::fill && options.paint != path_paint::stroke) || (options.fill != path_fill::nonzero && options.fill != path_fill::even_odd) || (options.join != path_join::miter && options.join != path_join::bevel && options.join != path_join::round) || (options.cap != path_cap::butt && options.cap != path_cap::square && options.cap != path_cap::round)) return false;
            bool open = false;
            for (const auto& command : commands) {
                if (!finite(command.first) || !finite(command.second) || !finite(command.third) || !dh::isfinite(command.rotation)) return false;
                switch (command.verb) {
                    case path_verb::move: open = true; break;
                    case path_verb::close: if (!open) return false; open = false; break;
                    case path_verb::line:
                    case path_verb::quadratic:
                    case path_verb::cubic: if (!open) return false; break;
                    case path_verb::arc:
                        if (!open || command.second.x < 0.0f || command.second.y < 0.0f || (command.arc_size != D2D1_ARC_SIZE_SMALL && command.arc_size != D2D1_ARC_SIZE_LARGE) || (command.direction != D2D1_SWEEP_DIRECTION_CLOCKWISE && command.direction != D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE)) return false;
                        break;
                    default: return false;
                }
            }
            return true;
        }

        class triangle_sink final : public ID2D1TessellationSink {
        public:
            triangle_sink(dh::vector<D2D1_TRIANGLE>& triangles, dh::size_t limit) noexcept : m_triangles(triangles), m_limit(limit) {}
            ~triangle_sink() = default;
            triangle_sink(const triangle_sink&) = delete;
            triangle_sink& operator=(const triangle_sink&) = delete;
            triangle_sink(triangle_sink&&) = delete;
            triangle_sink& operator=(triangle_sink&&) = delete;
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** output) noexcept override {
                if (output == nullptr) return E_POINTER;
                *output = nullptr;
                if (id != __uuidof(IUnknown) && id != __uuidof(ID2D1TessellationSink)) return E_NOINTERFACE;
                *output = static_cast<ID2D1TessellationSink*>(this);
                AddRef();
                return S_OK;
            }
            ULONG STDMETHODCALLTYPE AddRef() noexcept override { return 1; }
            ULONG STDMETHODCALLTYPE Release() noexcept override { return 1; }
            void STDMETHODCALLTYPE AddTriangles(const D2D1_TRIANGLE* triangles, UINT32 count) noexcept override {
                if (m_full) return;
                if (count > m_limit - m_triangles.size()) { m_full = true; return; }
                for (UINT32 index = 0; index < count; ++index) m_triangles.push_back(triangles[index]);
            }
            HRESULT STDMETHODCALLTYPE Close() noexcept override { return m_full ? E_BOUNDS : S_OK; }
            [[nodiscard]] bool full() const noexcept { return m_full; }
        private:
            dh::vector<D2D1_TRIANGLE>& m_triangles;
            dh::size_t m_limit;
            bool m_full{false};
        };

        [[nodiscard]] dh::expected<ComPtr<ID2D1PathGeometry>, geometry_failure> create_path(ID2D1Factory& factory, dh::span<const path_command> commands, path_options options) noexcept {
            ComPtr<ID2D1PathGeometry> path;
            HRESULT hr = factory.CreatePathGeometry(path.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::path_create_failed, hr}};
            ComPtr<ID2D1GeometrySink> sink;
            hr = path->Open(sink.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::sink_open_failed, hr}};
            sink->SetFillMode(options.fill == path_fill::even_odd ? D2D1_FILL_MODE_ALTERNATE : D2D1_FILL_MODE_WINDING);
            bool open = false;
            for (const auto& command : commands) {
                switch (command.verb) {
                    case path_verb::move:
                        if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
                        sink->BeginFigure(point(command.first), options.paint == path_paint::fill ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
                        open = true;
                        break;
                    case path_verb::line: sink->AddLine(point(command.first)); break;
                    case path_verb::quadratic: sink->AddQuadraticBezier({point(command.first), point(command.second)}); break;
                    case path_verb::cubic: sink->AddBezier({point(command.first), point(command.second), point(command.third)}); break;
                    case path_verb::arc: sink->AddArc({point(command.first), {command.second.x, command.second.y}, command.rotation, command.direction, command.arc_size}); break;
                    case path_verb::close: sink->EndFigure(D2D1_FIGURE_END_CLOSED); open = false; break;
                }
            }
            if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
            hr = sink->Close();
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::sink_close_failed, hr}};
            return path;
        }

        [[nodiscard]] dh::expected<ComPtr<ID2D1PathGeometry>, geometry_failure> widen(ID2D1Factory& factory, ID2D1PathGeometry& path, path_options options, float tolerance) noexcept {
            D2D1_STROKE_STYLE_PROPERTIES properties{};
            const auto cap = options.cap == path_cap::round ? D2D1_CAP_STYLE_ROUND :
                options.cap == path_cap::square ? D2D1_CAP_STYLE_SQUARE : D2D1_CAP_STYLE_FLAT;
            properties.startCap = cap;
            properties.endCap = cap;
            properties.dashCap = cap;
            properties.lineJoin = options.join == path_join::round ? D2D1_LINE_JOIN_ROUND :
                options.join == path_join::bevel ? D2D1_LINE_JOIN_BEVEL : D2D1_LINE_JOIN_MITER;
            properties.miterLimit = options.miter_limit;
            properties.dashStyle = D2D1_DASH_STYLE_SOLID;
            ComPtr<ID2D1StrokeStyle> style;
            HRESULT hr = factory.CreateStrokeStyle(properties, nullptr, 0, style.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::stroke_create_failed, hr}};
            ComPtr<ID2D1PathGeometry> result;
            hr = factory.CreatePathGeometry(result.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::path_create_failed, hr}};
            ComPtr<ID2D1GeometrySink> sink;
            hr = result->Open(sink.GetAddressOf());
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::sink_open_failed, hr}};
            hr = path.Widen(options.thickness, style.Get(), nullptr, tolerance, sink.Get());
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::geometry_widen_failed, hr}};
            hr = sink->Close();
            if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::sink_close_failed, hr}};
            return result;
        }
    }

    dh::string_view to_string(geometry_error error) noexcept {
        switch (error) {
            case geometry_error::not_initialized: return "not_initialized";
            case geometry_error::invalid_geometry: return "invalid_geometry";
            case geometry_error::factory_create_failed: return "factory_create_failed";
            case geometry_error::path_create_failed: return "path_create_failed";
            case geometry_error::sink_open_failed: return "sink_open_failed";
            case geometry_error::sink_close_failed: return "sink_close_failed";
            case geometry_error::stroke_create_failed: return "stroke_create_failed";
            case geometry_error::geometry_widen_failed: return "geometry_widen_failed";
            case geometry_error::tessellation_failed: return "tessellation_failed";
            case geometry_error::vertex_limit: return "vertex_limit";
            case geometry_error::preparation_busy: return "preparation_busy";
        }
        return "unknown";
    }

    dh::expected<void, geometry_failure> path_geometry::initialize(dh::size_t maximum_triangles) noexcept {
        if (m_busy) return dh::unexpected{geometry_failure{geometry_error::preparation_busy}};
        if (maximum_triangles == 0 || maximum_triangles > k_max_geometry_vertices / 3)
            return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        direct2d_factory<ID2D1Factory> factory;
        const auto created = factory.initialize(D2D1_FACTORY_TYPE_SINGLE_THREADED);
        if (!created) return dh::unexpected{geometry_failure{geometry_error::factory_create_failed, created.error().hr}};
        m_triangles.reserve(maximum_triangles);
        m_maximum_triangles = maximum_triangles;
        m_factory = dh::move(factory);
        return {};
    }

    void path_geometry::shutdown() noexcept {
        if (m_busy) dh::fail_fast("path preparation is still in use", FAST_FAIL_INVALID_ARG);
        m_factory.reset();
        m_triangles = {};
        m_maximum_triangles = 0;
    }

    dh::expected<dh::span<const D2D1_TRIANGLE>, geometry_failure> path_geometry::build(dh::span<const path_command> commands, path_options options, float tolerance) noexcept {
        if (!m_factory) return dh::unexpected{geometry_failure{geometry_error::not_initialized}};
        if (!valid(commands, options) || !(tolerance > 0.0f) || !dh::isfinite(tolerance))
            return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        m_triangles.clear();
        const auto path = create_path(*m_factory.get(), commands, options);
        if (!path) return dh::unexpected{path.error()};
        ComPtr<ID2D1PathGeometry> geometry = *path;
        if (options.paint == path_paint::stroke) {
            const auto stroke = widen(*m_factory.get(), *geometry.Get(), options, tolerance);
            if (!stroke) return dh::unexpected{stroke.error()};
            geometry = *stroke;
        }
        triangle_sink sink{m_triangles, m_maximum_triangles};
        const HRESULT hr = geometry->Tessellate(nullptr, tolerance, &sink);
        if (sink.full()) return dh::unexpected{geometry_failure{geometry_error::vertex_limit, E_BOUNDS}};
        if (FAILED(hr)) return dh::unexpected{geometry_failure{geometry_error::tessellation_failed, hr}};
        for (const auto& triangle : m_triangles) {
            for (const auto value : {triangle.point1, triangle.point2, triangle.point3})
                if (!dh::isfinite(value.x) || !dh::isfinite(value.y)) return dh::unexpected{geometry_failure{geometry_error::invalid_geometry, E_INVALIDARG}};
        }
        return dh::span<const D2D1_TRIANGLE>{m_triangles};
    }

    dh::expected<path_geometry::preparation, geometry_failure> path_geometry::prepare(dh::span<const path_command> commands, path_options options, float tolerance) noexcept {
        if (m_busy) return dh::unexpected{geometry_failure{geometry_error::preparation_busy}};
        const auto result = build(commands, options, tolerance);
        if (!result) return dh::unexpected{result.error()};
        m_busy = true;
        return preparation{*this, *result};
    }
}
