// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/span.h"
#include "core/dh/vector.h"
#include "math/vector2.h"
#include "render/direct2d_factory.h"

#include <d2d1.h>
#include <wrl/client.h>

namespace dh::render {

    inline constexpr dh::size_t k_max_geometry_vertices = 1048576;

    enum class geometry_error : dh::u8 {
        not_initialized, invalid_geometry, factory_create_failed, path_create_failed,
        sink_open_failed, sink_close_failed, stroke_create_failed, geometry_widen_failed,
        tessellation_failed, vertex_limit, preparation_busy,
    };
    [[nodiscard]] dh::string_view to_string(geometry_error error) noexcept;

    struct geometry_failure final {
    public:
        constexpr geometry_failure(geometry_error code_value, HRESULT result = S_OK) noexcept : code(code_value), hr(result) {}
        ~geometry_failure() = default;
        geometry_failure(const geometry_failure&) = default;
        geometry_failure& operator=(const geometry_failure&) = default;
        geometry_failure(geometry_failure&&) noexcept = default;
        geometry_failure& operator=(geometry_failure&&) noexcept = default;
        geometry_error code;
        HRESULT hr;
    };

    enum class path_verb : dh::u8 { move, line, quadratic, cubic, arc, close };
    enum class path_paint : dh::u8 { fill, stroke };
    enum class path_fill : dh::u8 { nonzero, even_odd };
    enum class path_join : dh::u8 { miter, bevel, round };
    enum class path_cap : dh::u8 { butt, square, round };

    struct path_command final {
    public:
        path_command() noexcept = default;
        ~path_command() = default;
        path_command(const path_command&) = default;
        path_command& operator=(const path_command&) = default;
        path_command(path_command&&) noexcept = default;
        path_command& operator=(path_command&&) noexcept = default;
        path_verb verb{path_verb::move};
        math::vector2 first{}, second{}, third{};
        float rotation{};
        D2D1_ARC_SIZE arc_size{D2D1_ARC_SIZE_SMALL};
        D2D1_SWEEP_DIRECTION direction{D2D1_SWEEP_DIRECTION_CLOCKWISE};
    };

    struct path_options final {
    public:
        path_options() noexcept = default;
        ~path_options() = default;
        path_options(const path_options&) = default;
        path_options& operator=(const path_options&) = default;
        path_options(path_options&&) noexcept = default;
        path_options& operator=(path_options&&) noexcept = default;
        path_paint paint{path_paint::fill};
        path_fill fill{path_fill::nonzero};
        path_join join{path_join::miter};
        path_cap cap{path_cap::butt};
        float thickness{1.0f};
        float miter_limit{10.0f};
    };

    class path_geometry final {
    public:
        class preparation final {
        public:
            ~preparation() { if (m_owner) m_owner->m_busy = false; }
            preparation(const preparation&) = delete;
            preparation& operator=(const preparation&) = delete;
            preparation(preparation&& other) noexcept : m_owner(other.m_owner), m_triangles(other.m_triangles) { other.m_owner = nullptr; }
            preparation& operator=(preparation&& other) noexcept {
                if (this == &other) return *this;
                if (m_owner) m_owner->m_busy = false;
                m_owner = other.m_owner;
                m_triangles = other.m_triangles;
                other.m_owner = nullptr;
                return *this;
            }
            [[nodiscard]] dh::span<const D2D1_TRIANGLE> triangles() const noexcept { return m_triangles; }
        private:
            friend class path_geometry;
            preparation(path_geometry& owner, dh::span<const D2D1_TRIANGLE> triangles) noexcept : m_owner(&owner), m_triangles(triangles) {}
            path_geometry* m_owner;
            dh::span<const D2D1_TRIANGLE> m_triangles;
        };

        path_geometry() noexcept = default;
        ~path_geometry() = default;
        path_geometry(const path_geometry&) = delete;
        path_geometry& operator=(const path_geometry&) = delete;
        path_geometry(path_geometry&&) = delete;
        path_geometry& operator=(path_geometry&&) = delete;
        [[nodiscard]] dh::expected<void, geometry_failure> initialize(dh::size_t maximum_triangles) noexcept;
        void shutdown() noexcept;
        [[nodiscard]] dh::expected<preparation, geometry_failure> prepare(dh::span<const path_command> commands, path_options options, float tolerance) noexcept;

        static constexpr dh::size_t k_max_commands = 4096;

    private:
        [[nodiscard]] dh::expected<dh::span<const D2D1_TRIANGLE>, geometry_failure> build(dh::span<const path_command> commands, path_options options, float tolerance) noexcept;
        direct2d_factory<ID2D1Factory> m_factory{};
        dh::vector<D2D1_TRIANGLE> m_triangles{};
        dh::size_t m_maximum_triangles{};
        bool m_busy{false};
    };

}
