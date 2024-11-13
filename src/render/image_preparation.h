// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "render/image_decoder.h"
#include "core/dh/optional.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"

namespace dh::render::image {

    inline constexpr dh::size_t k_max_preparation_jobs = 16;
    inline constexpr dh::size_t k_max_preparation_input_bytes = k_max_preparation_jobs * k_max_source_bytes;
    inline constexpr dh::size_t k_max_preparation_output_bytes = 2 * k_max_decoded_bytes;

    inline constexpr dh::size_t k_max_preparation_jobs_per_owner = k_max_preparation_jobs / 2;
    inline constexpr dh::size_t k_max_preparation_output_bytes_per_owner = k_max_preparation_output_bytes / 2;

    enum class preparation_error : dh::u8 {
        unavailable, allocation_failed, event_create_failed, worker_create_failed,
        invalid_request, queue_full, owner_limit, input_limit, output_limit, owner_output_limit, expired,
        decode_failed, source_failed, source_unavailable, source_timeout, invalid_pixels,
    };
    [[nodiscard]] dh::string to_string(preparation_error error) noexcept;

    struct preparation_failure final {
    public:
        preparation_failure(preparation_error code_value, dh::u32 detail_value = 0) noexcept : code(code_value), detail(detail_value) {}
        explicit preparation_failure(decode_failure error) noexcept : code(preparation_error::decode_failed),
            detail(static_cast<dh::u32>(error.hresult)), decoder(error.cause) {}
        ~preparation_failure() = default;
        preparation_failure(const preparation_failure&) = default;
        preparation_failure& operator=(const preparation_failure&) = default;
        preparation_failure(preparation_failure&&) noexcept = default;
        preparation_failure& operator=(preparation_failure&&) noexcept = default;
        preparation_error code;
        dh::u32 detail{};
        decode_error decoder{decode_error::source_empty};

        dh::string source_reason{};
    };

    struct preparation_id final {
    public:
        preparation_id() noexcept = default;
        preparation_id(dh::size_t index, dh::u64 generation) noexcept : slot(index), serial(generation) {}
        ~preparation_id() = default;
        preparation_id(const preparation_id&) = default;
        preparation_id& operator=(const preparation_id&) = default;
        preparation_id(preparation_id&&) noexcept = default;
        preparation_id& operator=(preparation_id&&) noexcept = default;
        dh::size_t slot{};
        dh::u64 serial{};
    };

    enum class preparation_status : dh::u8 { pending, ready, unavailable, failed };
    enum class preparation_activation : dh::u8 { staged, active };
    enum class preparation_thread : dh::u8 { main, render, worker };

    struct preparation_info final {
    public:
        preparation_info() noexcept = default;
        ~preparation_info() = default;
        preparation_info(const preparation_info&) = default;
        preparation_info& operator=(const preparation_info&) = default;
        preparation_info(preparation_info&&) noexcept = default;
        preparation_info& operator=(preparation_info&&) noexcept = default;
        preparation_status status{preparation_status::pending};
        dh::u32 width{}, height{};

        dh::size_t bytes{};
        dh::optional<preparation_failure> failure{};
    };

    struct external_image_sample final {
    public:
        external_image_sample() noexcept = default;
        ~external_image_sample() = default;
        external_image_sample(const external_image_sample&) = delete;
        external_image_sample& operator=(const external_image_sample&) = delete;
        external_image_sample(external_image_sample&&) noexcept = default;
        external_image_sample& operator=(external_image_sample&&) noexcept = default;
        preparation_status status{preparation_status::pending};
        decoded_image image{};
    };

    class external_image_source {
    public:
        external_image_source() noexcept = default;
        virtual ~external_image_source() = default;
        external_image_source(const external_image_source&) = delete;
        external_image_source& operator=(const external_image_source&) = delete;
        external_image_source(external_image_source&&) = delete;
        external_image_source& operator=(external_image_source&&) = delete;
        [[nodiscard]] virtual dh::expected<external_image_sample, preparation_failure> poll() noexcept = 0;
    };

    class image_preparation final {
    public:
        image_preparation() noexcept;
        ~image_preparation();
        image_preparation(const image_preparation&) = delete;
        image_preparation& operator=(const image_preparation&) = delete;
        image_preparation(image_preparation&&) = delete;
        image_preparation& operator=(image_preparation&&) = delete;
        [[nodiscard]] dh::expected<void, preparation_failure> initialize() noexcept;
        // join the worker before freeing its requests.
        void shutdown() noexcept;
        [[nodiscard]] dh::expected<preparation_id, preparation_failure> submit(const void* owner, preparation_activation activation, dh::string bytes, format source_format) noexcept;
        [[nodiscard]] dh::expected<preparation_id, preparation_failure> submit_external(const void* owner, preparation_activation activation, dh::unique_ptr<external_image_source> source, preparation_thread thread, dh::u64 interval_ms, dh::u64 timeout_ms) noexcept;
        void activate_owner(const void* owner) noexcept;
        void discard_owner(const void* owner) noexcept;
        void discard(preparation_id id) noexcept;
        [[nodiscard]] dh::expected<preparation_info, preparation_failure> inspect(preparation_id id) const noexcept;

        [[nodiscard]] dh::expected<dh::optional<decoded_image>, preparation_failure> take(preparation_id id) noexcept;
        void pump_external(preparation_thread thread) noexcept;

    private:
        class state;
        dh::unique_ptr<state> m_state{};
    };
}
