// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/filesystem.h"
#include "core/dh/io/io_error.h"
#include "core/dh/span.h"
#include "core/dh/string.h"

namespace dh::io {

    enum class open_mode : unsigned char {
        truncate,
        append,
    };

    class file_writer final {
    public:
        [[nodiscard]] static dh::expected<file_writer, io_failure> open(const dh::fs::path& target, open_mode mode) noexcept;

        file_writer() noexcept = default;
        ~file_writer();

        file_writer(const file_writer&) = delete;
        file_writer& operator=(const file_writer&) = delete;

        file_writer(file_writer&& other) noexcept;
        file_writer& operator=(file_writer&& other) noexcept;

        [[nodiscard]] dh::expected<void, io_failure> write(dh::span<const dh::u8> bytes) noexcept;

        [[nodiscard]] dh::expected<void, io_failure> write_chars(dh::span<const char> chars) noexcept;

        [[nodiscard]] dh::expected<void, io_failure> write_line(dh::string_view line) noexcept;

        [[nodiscard]] dh::expected<void, io_failure> flush() noexcept;

        void close() noexcept;

        [[nodiscard]] bool is_open() const noexcept;

        [[nodiscard]] void* native_handle() const noexcept { return m_handle; }

    private:
        explicit file_writer(void* handle) noexcept;

        void* m_handle{nullptr};
    };

}
