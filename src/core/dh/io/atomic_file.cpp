// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "core/dh/io/atomic_file.h"

#include "core/dh/io/file_writer.h"
#include "core/dh/memory.h"
#include "core/dh/vector.h"

#include <Windows.h>

namespace dh::io {

    namespace {

        constexpr wchar_t k_temp_suffix[] = L".tmp";
        constexpr dh::size_t k_temp_suffix_length = (sizeof(k_temp_suffix) / sizeof(wchar_t)) - 1;

        constexpr int k_rename_attempts = 10;
        constexpr DWORD k_rename_retry_milliseconds = 10;

        [[nodiscard]] dh::fs::path companion_path(const dh::fs::path& target) noexcept {
            const dh::size_t target_length = target.size();
            dh::vector<wchar_t> buffer;
            buffer.resize(target_length + k_temp_suffix_length + 1);
            dh::memcpy(buffer.data(), target.c_str(), target_length * sizeof(wchar_t));
            dh::memcpy(buffer.data() + target_length, k_temp_suffix, k_temp_suffix_length * sizeof(wchar_t));
            buffer[target_length + k_temp_suffix_length] = L'\0';
            return dh::fs::path{buffer.data(), target_length + k_temp_suffix_length};
        }

        [[nodiscard]] bool rename_over(const dh::fs::path& from, const dh::fs::path& to, DWORD& last_error) noexcept {
            for (int attempt = 0; attempt < k_rename_attempts; ++attempt) {
                if (::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) return true;
                last_error = ::GetLastError();
                if (last_error != ERROR_SHARING_VIOLATION && last_error != ERROR_ACCESS_DENIED) return false;
                if (attempt + 1 < k_rename_attempts) ::Sleep(k_rename_retry_milliseconds);
            }
            return false;
        }

        [[nodiscard]] dh::unexpected<atomic_write_failure> fail(atomic_write_error cause, dh::u32 os_error, dh::u32 cleanup_os_error = 0) noexcept {
            return dh::unexpected<atomic_write_failure>{atomic_write_failure{cause, os_error, cleanup_os_error}};
        }

    }

    dh::expected<void, atomic_recovery_failure> discard_atomic_companion(const dh::fs::path& target) noexcept {
        const dh::fs::path companion = companion_path(target);
        const DWORD attributes = ::GetFileAttributesW(companion.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = ::GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return {};
            return dh::unexpected{atomic_recovery_failure{atomic_recovery_error::inspect_failed, error}};
        }
        if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
            return dh::unexpected{atomic_recovery_failure{atomic_recovery_error::not_regular_file,
                static_cast<dh::u32>((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ? ERROR_REPARSE_TAG_INVALID : ERROR_DIRECTORY)}};
        }
        if (::DeleteFileW(companion.c_str()) == FALSE) {
            return dh::unexpected{atomic_recovery_failure{atomic_recovery_error::remove_failed, ::GetLastError()}};
        }
        return {};
    }

    dh::expected<void, atomic_write_failure> write_atomic(const dh::fs::path& target, dh::span<const char> bytes) noexcept {
        const dh::fs::path companion = companion_path(target);
        const DWORD companion_attributes = ::GetFileAttributesW(companion.c_str());
        if (companion_attributes != INVALID_FILE_ATTRIBUTES && (companion_attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return fail(atomic_write_error::temp_reparse_refused, ERROR_REPARSE_TAG_INVALID);
        }
        // close the flushed temp file before renaming it over the target.
        atomic_write_error body_failure = atomic_write_error::temp_write_failed;
        dh::u32 body_error = 0;
        bool body_failed = false;
        {
            auto sink = file_writer::open(companion, open_mode::truncate);
            if (!sink) return fail(atomic_write_error::temp_open_failed, sink.error().os_error);
            if (auto written = sink->write_chars(bytes); !written) {
                body_failure = atomic_write_error::temp_write_failed;
                body_error = written.error().os_error;
                body_failed = true;
            } else if (auto flushed = sink->flush(); !flushed) {
                body_failure = atomic_write_error::temp_flush_failed;
                body_error = flushed.error().os_error;
                body_failed = true;
            }
        }
        if (body_failed) {
            dh::u32 cleanup_error = 0;
            if (auto removed = dh::fs::remove(companion); !removed) cleanup_error = removed.error().os_error;
            return fail(body_failure, body_error, cleanup_error);
        }

        DWORD rename_error = 0;
        if (rename_over(companion, target, rename_error)) return {};

        dh::u32 cleanup_error = 0;
        if (auto removed = dh::fs::remove(companion); !removed) cleanup_error = removed.error().os_error;
        return fail(atomic_write_error::rename_failed, static_cast<dh::u32>(rename_error), cleanup_error);
    }

}
