// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/filesystem.h"
#include "core/dh/span.h"

#include "core/dh/memory.h"

namespace dh::io {

    enum class atomic_recovery_error : dh::u8 {
        inspect_failed,
        not_regular_file,
        remove_failed,
    };

    [[nodiscard]] constexpr dh::string_view to_string(atomic_recovery_error error) noexcept {
        switch (error) {
            case atomic_recovery_error::inspect_failed: return "inspect_failed";
            case atomic_recovery_error::not_regular_file: return "not_regular_file";
            case atomic_recovery_error::remove_failed: return "remove_failed";
        }
        return "unknown";
    }

    struct atomic_recovery_failure final {
    public:
        atomic_recovery_failure(atomic_recovery_error cause_in, dh::u32 os_error_in) noexcept : cause(cause_in), os_error(os_error_in) {}
        ~atomic_recovery_failure() = default;
        atomic_recovery_failure(const atomic_recovery_failure&) = default;
        atomic_recovery_failure& operator=(const atomic_recovery_failure&) = default;
        atomic_recovery_failure(atomic_recovery_failure&&) noexcept = default;
        atomic_recovery_failure& operator=(atomic_recovery_failure&&) noexcept = default;

        atomic_recovery_error cause;
        dh::u32 os_error;
    };

    [[nodiscard]] dh::expected<void, atomic_recovery_failure> discard_atomic_companion(const dh::fs::path& target) noexcept;

    enum class atomic_write_error : dh::u8 {
        temp_reparse_refused,
        temp_open_failed,
        temp_write_failed,
        temp_flush_failed,
        rename_failed,
    };

    [[nodiscard]] constexpr dh::string_view to_string(atomic_write_error error) noexcept {
        switch (error) {
            case atomic_write_error::temp_reparse_refused:
                return "temp_reparse_refused";
            case atomic_write_error::temp_open_failed:
                return "temp_open_failed";
            case atomic_write_error::temp_write_failed:
                return "temp_write_failed";
            case atomic_write_error::temp_flush_failed:
                return "temp_flush_failed";
            case atomic_write_error::rename_failed:
                return "rename_failed";
        }
        return "unknown";
    }

    struct atomic_write_failure final {
    public:
        atomic_write_failure(atomic_write_error cause_in, dh::u32 os_error_in, dh::u32 cleanup_os_error_in = 0) noexcept : cause(cause_in), os_error(os_error_in), cleanup_os_error(cleanup_os_error_in) {}
        ~atomic_write_failure() = default;
        atomic_write_failure(const atomic_write_failure&) = default;
        atomic_write_failure& operator=(const atomic_write_failure&) = default;
        atomic_write_failure(atomic_write_failure&&) noexcept = default;
        atomic_write_failure& operator=(atomic_write_failure&&) noexcept = default;

        atomic_write_error cause;
        dh::u32 os_error;

        dh::u32 cleanup_os_error;
    };

    [[nodiscard]] dh::expected<void, atomic_write_failure> write_atomic(const dh::fs::path& target, dh::span<const char> bytes) noexcept;

}
