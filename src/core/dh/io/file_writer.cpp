// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "core/dh/io/file_writer.h"

namespace dh::io {

    namespace {

        [[nodiscard]] bool handle_is_valid(void* h) noexcept {
            return h != nullptr && h != INVALID_HANDLE_VALUE;
        }

    }

    dh::expected<file_writer, io_failure> file_writer::open(const dh::fs::path& target, open_mode mode) noexcept {
        const DWORD disposition = (mode == open_mode::truncate) ? static_cast<DWORD>(CREATE_ALWAYS) : static_cast<DWORD>(OPEN_ALWAYS);

        HANDLE raw = ::CreateFileW(target.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (raw == INVALID_HANDLE_VALUE) {
            return dh::unexpected<io_failure>{io_failure{io_error::open_failed, static_cast<dh::u32>(::GetLastError())}};
        }

        return file_writer{raw};
    }

    file_writer::file_writer(void* handle) noexcept : m_handle(handle) {}

    file_writer::~file_writer() {
        close();
    }

    file_writer::file_writer(file_writer&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    file_writer& file_writer::operator=(file_writer&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

    namespace {

        [[nodiscard]] dh::expected<void, io_failure> write_raw_bytes(void* handle, const void* data, dh::size_t bytes) noexcept {
            if (bytes == 0) {
                return {};
            }
            const auto* cursor = static_cast<const unsigned char*>(data);
            dh::size_t remaining = bytes;

            while (remaining > 0) {
                const DWORD chunk = remaining > static_cast<dh::size_t>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(remaining);
                DWORD written = 0;
                const BOOL ok = ::WriteFile(handle, cursor, chunk, &written, nullptr);
                if (ok == FALSE || written == 0) {
                    const dh::u32 os_error = (ok == FALSE) ? static_cast<dh::u32>(::GetLastError()) : dh::u32{0};
                    return dh::unexpected<io_failure>{io_failure{io_error::write_failed, os_error}};
                }
                cursor += written;
                remaining -= static_cast<dh::size_t>(written);
            }
            return {};
        }

    }

    dh::expected<void, io_failure> file_writer::write(dh::span<const dh::u8> bytes) noexcept {
        if (!handle_is_valid(m_handle)) {
            return dh::unexpected<io_failure>{io_failure{io_error::already_closed, 0}};
        }
        return write_raw_bytes(m_handle, bytes.data(), bytes.size());
    }

    dh::expected<void, io_failure> file_writer::write_chars(dh::span<const char> chars) noexcept {
        if (!handle_is_valid(m_handle)) {
            return dh::unexpected<io_failure>{io_failure{io_error::already_closed, 0}};
        }
        return write_raw_bytes(m_handle, chars.data(), chars.size());
    }

    dh::expected<void, io_failure> file_writer::write_line(dh::string_view line) noexcept {
        if (!handle_is_valid(m_handle)) {
            return dh::unexpected<io_failure>{io_failure{io_error::already_closed, 0}};
        }

        if (line.size() > 0) {
            auto result = write_raw_bytes(m_handle, line.data(), line.size());
            if (!result.has_value()) {
                return result;
            }
        }

        const char newline = '\n';
        return write_raw_bytes(m_handle, &newline, dh::size_t{1});
    }

    dh::expected<void, io_failure> file_writer::flush() noexcept {
        if (!handle_is_valid(m_handle)) {
            return dh::unexpected<io_failure>{io_failure{io_error::already_closed, 0}};
        }
        if (::FlushFileBuffers(m_handle) == FALSE) {
            return dh::unexpected<io_failure>{io_failure{io_error::flush_failed, static_cast<dh::u32>(::GetLastError())}};
        }
        return {};
    }

    void file_writer::close() noexcept {
        if (handle_is_valid(m_handle)) {
            ::CloseHandle(m_handle);
        }
        m_handle = nullptr;
    }

    bool file_writer::is_open() const noexcept {
        return handle_is_valid(m_handle);
    }

}
