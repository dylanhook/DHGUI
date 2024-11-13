// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "core/dh/io/file_reader.h"

namespace dh::io {

    namespace {

        [[nodiscard]] bool handle_is_valid(void* h) noexcept {
            return h != nullptr && h != INVALID_HANDLE_VALUE;
        }

        [[nodiscard]] dh::unexpected<io_failure> fail(io_error step, dh::u32 os_error) noexcept {
            return dh::unexpected<io_failure>{io_failure{step, os_error}};
        }

    }

    dh::expected<file_reader, io_failure> file_reader::open(const dh::fs::path& target) noexcept {
        HANDLE raw = ::CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (raw == INVALID_HANDLE_VALUE) {
            return fail(io_error::open_failed, static_cast<dh::u32>(::GetLastError()));
        }
        return file_reader{raw};
    }

    file_reader::file_reader(void* handle) noexcept : m_handle(handle) {}

    file_reader::~file_reader() {
        close();
    }

    file_reader::file_reader(file_reader&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    file_reader& file_reader::operator=(file_reader&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

    dh::expected<dh::u64, io_failure> file_reader::size() const noexcept {
        if (!handle_is_valid(m_handle)) {
            return fail(io_error::already_closed, 0);
        }

        LARGE_INTEGER size{};
        if (::GetFileSizeEx(m_handle, &size) == FALSE) {
            return fail(io_error::size_query_failed, static_cast<dh::u32>(::GetLastError()));
        }

        if (size.QuadPart < 0) {
            return fail(io_error::too_large, 0);
        }
        return static_cast<dh::u64>(size.QuadPart);
    }

    dh::expected<dh::string, io_failure> file_reader::read_all(dh::size_t max_bytes) noexcept {
        const auto extent = size();
        if (!extent) return dh::unexpected{extent.error()};
        if (*extent > max_bytes) return fail(io_error::too_large, 0);
        return read_extent({0, static_cast<dh::size_t>(*extent)}, true);
    }

    dh::expected<dh::string, io_failure> file_reader::read_at(file_range range) noexcept {
        const auto extent = size();
        if (!extent) return dh::unexpected{extent.error()};
        if (range.offset > *extent) return fail(io_error::range_invalid, 0);
        const auto remaining = *extent - range.offset;
        if (range.length > remaining) range.length = static_cast<dh::size_t>(remaining);
        return read_extent(range, false);
    }

    dh::expected<dh::string, io_failure> file_reader::read_extent(file_range range, bool verify_end) noexcept {
        LARGE_INTEGER beginning{};
        beginning.QuadPart = static_cast<LONGLONG>(range.offset);
        if (::SetFilePointerEx(m_handle, beginning, nullptr, FILE_BEGIN) == FALSE) {
            return fail(io_error::read_failed, static_cast<dh::u32>(::GetLastError()));
        }

        dh::string out;
        out.resize(range.length);

        char* cursor = out.data();
        dh::size_t remaining = range.length;
        while (remaining > 0) {
            const DWORD chunk = remaining > static_cast<dh::size_t>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(remaining);
            DWORD read = 0;
            const BOOL ok = ::ReadFile(m_handle, cursor, chunk, &read, nullptr);
            if (ok == FALSE || read == 0) {
                const dh::u32 os_error = (ok == FALSE) ? static_cast<dh::u32>(::GetLastError()) : dh::u32{0};
                return fail(io_error::read_failed, os_error);
            }
            cursor += read;
            remaining -= static_cast<dh::size_t>(read);
        }
        if (!verify_end) return out;
        char extra = 0;
        DWORD read = 0;
        if (::ReadFile(m_handle, &extra, 1, &read, nullptr) == FALSE) {
            return fail(io_error::read_failed, static_cast<dh::u32>(::GetLastError()));
        }
        if (read != 0) return fail(io_error::file_changed, 0);
        return out;
    }

    void file_reader::close() noexcept {
        if (handle_is_valid(m_handle)) {
            ::CloseHandle(m_handle);
        }
        m_handle = nullptr;
    }

    bool file_reader::is_open() const noexcept {
        return handle_is_valid(m_handle);
    }

}
