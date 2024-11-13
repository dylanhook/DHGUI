// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/filesystem.h"
#include "core/dh/io/io_error.h"
#include "core/dh/string.h"

namespace dh::io {

    struct file_range final {
        dh::u64 offset{0};
        dh::size_t length{0};
    };

    class file_reader final {
    public:
        [[nodiscard]] static dh::expected<file_reader, io_failure> open(const dh::fs::path& target) noexcept;

        file_reader() noexcept = default;
        ~file_reader();

        file_reader(const file_reader&) = delete;
        file_reader& operator=(const file_reader&) = delete;

        file_reader(file_reader&& other) noexcept;
        file_reader& operator=(file_reader&& other) noexcept;

        [[nodiscard]] dh::expected<dh::string, io_failure> read_all(dh::size_t max_bytes) noexcept;

        [[nodiscard]] dh::expected<dh::string, io_failure> read_at(file_range range) noexcept;

        void close() noexcept;

        [[nodiscard]] bool is_open() const noexcept;

    private:
        explicit file_reader(void* handle) noexcept;

        [[nodiscard]] dh::expected<dh::u64, io_failure> size() const noexcept;
        [[nodiscard]] dh::expected<dh::string, io_failure> read_extent(file_range range, bool verify_end) noexcept;

        void* m_handle{nullptr};
    };

}
