// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "expected.h"
#include "memory.h"
#include "string.h"
#include "vector.h"
#include "core/dh/type_traits.h"

namespace dh::fs {

    class path final {
    public:
        using size_type = dh::size_t;
        using value_type = wchar_t;
        using string_type = dh::vector<wchar_t>;

        constexpr path() noexcept = default;
        path(const path&) noexcept;
        path(path&&) noexcept;
        path& operator=(const path&) noexcept;
        path& operator=(path&&) noexcept;
        ~path();

        path(const wchar_t* w) noexcept;
        path(const wchar_t* w, size_type len) noexcept;
        path(const char* u8) noexcept;
        path(const dh::string& u8) noexcept;
        path(dh::string_view u8) noexcept;

        template <typename WV, typename = decltype(static_cast<const wchar_t*>(dh::declval<const WV&>().data()))> path(const WV& wv) noexcept : path(wv.data(), static_cast<size_type>(wv.size())) {}

        [[nodiscard]] const wchar_t* c_str() const noexcept;

        [[nodiscard]] const wchar_t* data() const noexcept { return c_str(); }

        [[nodiscard]] size_type size() const noexcept;

        [[nodiscard]] bool empty() const noexcept { return size() == 0; }

        [[nodiscard]] dh::string u8string() const noexcept;

        [[nodiscard]] const wchar_t* native() const noexcept { return c_str(); }

        path& operator/=(const path& p) noexcept;
        path& operator/=(const wchar_t* w) noexcept;
        path& operator/=(const char* u8) noexcept;

        [[nodiscard]] friend path operator/(path a, const path& b) noexcept { return a /= b; }

        [[nodiscard]] friend path operator/(path a, const wchar_t* b) noexcept { return a /= b; }

        [[nodiscard]] friend path operator/(path a, const char* b) noexcept { return a /= b; }

        [[nodiscard]] path parent_path() const noexcept;

        [[nodiscard]] path filename() const noexcept;

        [[nodiscard]] path stem() const noexcept;

        [[nodiscard]] path extension() const noexcept;
        [[nodiscard]] bool has_extension() const noexcept;

        friend bool operator==(const path& a, const path& b) noexcept;

        friend bool operator!=(const path& a, const path& b) noexcept { return !(a == b); }

    private:
        string_type m_native;
        void normalize_separators() noexcept;
    };

    enum class fs_error : dh::u8 {
        not_found,
        access_denied,
        path_too_long,
        name_invalid,
        disk_full,
        in_use,
        not_empty,
        io_failed,
    };

    [[nodiscard]] constexpr dh::string_view to_string(fs_error e) noexcept {
        switch (e) {
            case fs_error::not_found:
                return "not_found";
            case fs_error::access_denied:
                return "access_denied";
            case fs_error::path_too_long:
                return "path_too_long";
            case fs_error::name_invalid:
                return "name_invalid";
            case fs_error::disk_full:
                return "disk_full";
            case fs_error::in_use:
                return "in_use";
            case fs_error::not_empty:
                return "not_empty";
            case fs_error::io_failed:
                return "io_failed";
        }
        return "unknown";
    }

    struct fs_failure final {
        fs_error cause;
        dh::u32 os_error;
    };

    struct file_status final {
        bool exists{false};
        bool is_directory{false};
        dh::u64 size_bytes{0};
        dh::u64 last_write_filetime{0};
    };

    [[nodiscard]] dh::expected<file_status, fs_failure> status(const path& p) noexcept;

    [[nodiscard]] dh::expected<bool, fs_failure> exists(const path& p) noexcept;

    [[nodiscard]] dh::expected<bool, fs_failure> equivalent(const path& left, const path& right) noexcept;

    [[nodiscard]] dh::expected<void, fs_failure> create_directories(const path& p) noexcept;

    [[nodiscard]] dh::expected<void, fs_failure> remove(const path& p) noexcept;

    class directory_iterator final {
    public:
        struct entry final {
            path file_path;
            bool is_directory{false};

            bool is_reparse_point{false};
            dh::u64 size_bytes{0};
            dh::u64 last_write_filetime{0};
        };

        [[nodiscard]] static dh::expected<directory_iterator, fs_failure> open(const path& dir) noexcept;

        directory_iterator() noexcept = default;
        directory_iterator(const directory_iterator&) = delete;
        directory_iterator(directory_iterator&& other) noexcept;
        directory_iterator& operator=(const directory_iterator&) = delete;
        directory_iterator& operator=(directory_iterator&&) noexcept;
        ~directory_iterator();

        [[nodiscard]] bool at_end() const noexcept { return m_handle == nullptr; }

        [[nodiscard]] const entry& current() const noexcept { return m_entry; }

        [[nodiscard]] dh::expected<void, fs_failure> advance() noexcept;

        const entry& operator*() const noexcept { return m_entry; }

    private:
        void* m_handle{nullptr};
        path m_root;
        entry m_entry;
        void load_current_from_find_data(const void* find_data) noexcept;
    };

    enum class watch_state : dh::u8 {
        unchanged,
        changed,
        watch_lost,
    };

    enum class watch_filter : dh::u8 {
        names,
        names_and_writes,
    };

    enum class watch_scope : dh::u8 { directory, subtree };

    class directory_watch final {
    public:
        [[nodiscard]] static dh::expected<directory_watch, fs_failure> arm(const path& dir, watch_filter filter, watch_scope scope) noexcept;

        directory_watch() noexcept = default;
        directory_watch(const directory_watch&) = delete;
        directory_watch& operator=(const directory_watch&) = delete;
        directory_watch(directory_watch&& other) noexcept;
        directory_watch& operator=(directory_watch&& other) noexcept;
        ~directory_watch();

        [[nodiscard]] bool armed() const noexcept { return m_handle != nullptr; }

        [[nodiscard]] watch_state consume_change() noexcept;

    private:
        void* m_handle{nullptr};
    };

}
