// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "filesystem.h"

#include "core/dh/utility.h"

namespace dh::fs {

    namespace {

        constexpr wchar_t k_sep = L'\\';

        class identity_query_handle final {
        public:
            explicit identity_query_handle(HANDLE handle) noexcept : m_handle(handle) {}
            ~identity_query_handle() { ::CloseHandle(m_handle); }
            identity_query_handle(const identity_query_handle&) = delete;
            identity_query_handle& operator=(const identity_query_handle&) = delete;
            identity_query_handle(identity_query_handle&&) = delete;
            identity_query_handle& operator=(identity_query_handle&&) = delete;
            [[nodiscard]] HANDLE get() const noexcept { return m_handle; }
        private:
            HANDLE m_handle;
        };

        [[nodiscard]] dh::size_t wide_length(const wchar_t* s) noexcept {
            if (s == nullptr) return 0;
            dh::size_t n = 0;
            while (s[n] != L'\0')
                ++n;
            return n;
        }

        [[nodiscard]] dh::vector<wchar_t> utf8_to_wide(const char* u8, dh::size_t bytes) noexcept {
            dh::vector<wchar_t> out;
            if (u8 == nullptr || bytes == 0) return out;
            if (bytes > INT_MAX) dh::fail_fast("dh::fs: UTF-8 length exceeds Win32 range", FAST_FAIL_INVALID_ARG);
            const int needed = ::MultiByteToWideChar(CP_UTF8, 0, u8, static_cast<int>(bytes), nullptr, 0);
            if (needed <= 0) dh::fail_fast("dh::fs: UTF-8 conversion failed", FAST_FAIL_INVALID_ARG);
            out.resize(static_cast<dh::size_t>(needed));
            if (::MultiByteToWideChar(CP_UTF8, 0, u8, static_cast<int>(bytes), out.data(), needed) != needed) {
                dh::fail_fast("dh::fs: UTF-8 conversion changed size", FAST_FAIL_INVALID_ARG);
            }
            return out;
        }

        [[nodiscard]] dh::string wide_to_utf8(const wchar_t* w, dh::size_t wide_chars) noexcept {
            if (w == nullptr || wide_chars == 0) return dh::string{};
            if (wide_chars > INT_MAX) dh::fail_fast("dh::fs: UTF-16 length exceeds Win32 range", FAST_FAIL_INVALID_ARG);
            const int needed = ::WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(wide_chars), nullptr, 0, nullptr, nullptr);
            if (needed <= 0) dh::fail_fast("dh::fs: UTF-16 conversion failed", FAST_FAIL_INVALID_ARG);
            dh::string out;
            out.resize(static_cast<dh::size_t>(needed));
            if (::WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(wide_chars), out.data(), needed, nullptr, nullptr) != needed) {
                dh::fail_fast("dh::fs: UTF-16 conversion changed size", FAST_FAIL_INVALID_ARG);
            }
            return out;
        }

        [[nodiscard]] dh::u64 filetime_to_u64(const FILETIME& ft) noexcept {
            return (static_cast<dh::u64>(ft.dwHighDateTime) << 32) | static_cast<dh::u64>(ft.dwLowDateTime);
        }

        [[nodiscard]] fs_failure failure_from(DWORD last_error) noexcept {
            fs_error cause = fs_error::io_failed;
            switch (last_error) {
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND:
                case ERROR_INVALID_DRIVE:
                case ERROR_BAD_NETPATH:
                    cause = fs_error::not_found;
                    break;
                case ERROR_ACCESS_DENIED:
                case ERROR_WRITE_PROTECT:
                case ERROR_NETWORK_ACCESS_DENIED:
                    cause = fs_error::access_denied;
                    break;
                case ERROR_FILENAME_EXCED_RANGE:
                case ERROR_BUFFER_OVERFLOW:
                    cause = fs_error::path_too_long;
                    break;
                case ERROR_INVALID_NAME:
                case ERROR_DIRECTORY:
                case ERROR_BAD_PATHNAME:
                    cause = fs_error::name_invalid;
                    break;
                case ERROR_DISK_FULL:
                case ERROR_HANDLE_DISK_FULL:
                    cause = fs_error::disk_full;
                    break;
                case ERROR_SHARING_VIOLATION:
                case ERROR_LOCK_VIOLATION:
                case ERROR_USER_MAPPED_FILE:
                    cause = fs_error::in_use;
                    break;
                case ERROR_DIR_NOT_EMPTY:
                    cause = fs_error::not_empty;
                    break;
                default:
                    break;
            }
            return fs_failure{cause, static_cast<dh::u32>(last_error)};
        }

        [[nodiscard]] dh::size_t root_length(const wchar_t* chars, dh::size_t n) noexcept {
            if (n >= 2 && chars[0] == k_sep && chars[1] == k_sep) {
                dh::size_t at = 2;
                if (n >= 4 && chars[2] == L'?' && chars[3] == k_sep) {
                    if (n >= 7 && chars[5] == L':' && chars[6] == k_sep) return 7;
                    if (n >= 8 && (chars[4] == L'U' || chars[4] == L'u') && (chars[5] == L'N' || chars[5] == L'n') && (chars[6] == L'C' || chars[6] == L'c') && chars[7] == k_sep)
                        at = 8;
                }
                for (int component = 0; component < 2; ++component) {
                    while (at < n && chars[at] != k_sep)
                        ++at;
                    if (at < n) ++at;
                }
                return at;
            }
            if (n >= 3 && chars[1] == L':' && chars[2] == k_sep) return 3;
            if (n >= 1 && chars[0] == k_sep) return 1;
            return 0;
        }

    }

    path::path(const path&) noexcept = default;
    path::path(path&&) noexcept = default;
    path& path::operator=(const path&) noexcept = default;
    path& path::operator=(path&&) noexcept = default;
    path::~path() = default;

    path::path(const wchar_t* w) noexcept : path(w, wide_length(w)) {}

    path::path(const wchar_t* w, size_type len) noexcept {
        if (w == nullptr || len == 0) return;

        if (len == static_cast<size_type>(-1)) dh::alloc_failed("dh::fs::path: size overflow");
        m_native.resize(len + 1);
        for (dh::size_t i = 0; i < len; ++i)
            m_native[i] = w[i];
        m_native[len] = L'\0';
        normalize_separators();
    }

    path::path(const char* u8) noexcept : path(u8 == nullptr ? dh::string_view{} : dh::string_view{u8}) {}

    path::path(const dh::string& u8) noexcept : path(dh::string_view{u8.data(), u8.size()}) {}

    path::path(dh::string_view u8) noexcept {
        auto wide = utf8_to_wide(u8.data(), u8.size());
        if (wide.empty()) return;
        wide.push_back(L'\0');
        m_native = static_cast<dh::vector<wchar_t>&&>(wide);
        normalize_separators();
    }

    const wchar_t* path::c_str() const noexcept {
        if (m_native.empty()) return L"";
        return m_native.data();
    }

    path::size_type path::size() const noexcept {
        return m_native.empty() ? dh::size_t{0} : m_native.size() - 1;
    }

    dh::string path::u8string() const noexcept {
        return wide_to_utf8(m_native.data(), size());
    }

    void path::normalize_separators() noexcept {
        for (dh::size_t i = 0, n = size(); i < n; ++i) {
            if (m_native[i] == L'/') m_native[i] = k_sep;
        }
    }

    path& path::operator/=(const path& p) noexcept {
        if (p.empty()) return *this;
        const dh::size_t self_len = size();
        const dh::size_t other_len = p.size();
        if (self_len == 0) {
            *this = p;
            return *this;
        }

        const bool needs_sep = m_native[self_len - 1] != k_sep;
        m_native.resize(self_len + (needs_sep ? 1 : 0) + other_len + 1);
        dh::size_t at = self_len;
        if (needs_sep) m_native[at++] = k_sep;
        for (dh::size_t i = 0; i < other_len; ++i)
            m_native[at++] = p.m_native[i];
        m_native[at] = L'\0';
        return *this;
    }

    path& path::operator/=(const wchar_t* w) noexcept {
        return operator/=(path{w});
    }

    path& path::operator/=(const char* u8) noexcept {
        return operator/=(path{u8});
    }

    path path::parent_path() const noexcept {
        const dh::size_t n = size();
        if (n == 0) return {};

        const dh::size_t root = root_length(m_native.data(), n);
        dh::size_t i = n;
        while (i > root && m_native[i - 1] != k_sep)
            --i;
        while (i > root && m_native[i - 1] == k_sep)
            --i;
        return path{m_native.data(), i};
    }

    path path::filename() const noexcept {
        const dh::size_t n = size();
        if (n == 0) return {};
        dh::size_t i = n;
        while (i > 0 && m_native[i - 1] != k_sep)
            --i;
        return path{m_native.data() + i, n - i};
    }

    path path::stem() const noexcept {
        const path name = filename();
        const path ext = extension();
        if (ext.empty()) return name;
        return path{name.m_native.data(), name.size() - ext.size()};
    }

    path path::extension() const noexcept {
        const dh::size_t n = size();
        dh::size_t begin = n;
        while (begin > 0 && m_native[begin - 1] != k_sep)
            --begin;
        if (begin == n) return {};
        if (n - begin == 2 && m_native[begin] == L'.' && m_native[begin + 1] == L'.') return {};
        dh::size_t dot = n;
        while (dot > begin && m_native[dot - 1] != L'.')
            --dot;
        if (dot <= begin + 1) return {};
        return path{m_native.data() + dot - 1, n - dot + 1};
    }

    bool path::has_extension() const noexcept {
        return !extension().empty();
    }

    bool operator==(const path& a, const path& b) noexcept {
        const dh::size_t n = a.size();
        if (n != b.size()) return false;
        for (dh::size_t i = 0; i < n; ++i) {
            if (a.m_native[i] != b.m_native[i]) return false;
        }
        return true;
    }

    dh::expected<file_status, fs_failure> status(const path& p) noexcept {
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (!::GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fad)) {
            const DWORD last_error = ::GetLastError();

            if (last_error == ERROR_FILE_NOT_FOUND || last_error == ERROR_PATH_NOT_FOUND) {
                return file_status{};
            }
            return dh::unexpected<fs_failure>{failure_from(last_error)};
        }
        file_status st{};
        st.exists = true;
        st.is_directory = (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        st.size_bytes = (static_cast<dh::u64>(fad.nFileSizeHigh) << 32) | static_cast<dh::u64>(fad.nFileSizeLow);
        st.last_write_filetime = filetime_to_u64(fad.ftLastWriteTime);
        return st;
    }

    dh::expected<bool, fs_failure> exists(const path& p) noexcept {
        auto st = status(p);
        if (!st) return dh::unexpected<fs_failure>{st.error()};
        return st->exists;
    }

    dh::expected<bool, fs_failure> equivalent(const path& left, const path& right) noexcept {
        const auto identify = [](const path& value) noexcept -> dh::expected<FILE_ID_INFO, fs_failure> {
            const HANDLE handle = ::CreateFileW(value.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
            if (handle == INVALID_HANDLE_VALUE) return dh::unexpected{failure_from(::GetLastError())};
            const identity_query_handle owned{handle};
            FILE_ID_INFO identity{};
            if (!::GetFileInformationByHandleEx(owned.get(), FileIdInfo, &identity, sizeof(identity)))
                return dh::unexpected{failure_from(::GetLastError())};
            return identity;
        };
        const auto first = identify(left);
        if (!first) return dh::unexpected{first.error()};
        const auto second = identify(right);
        if (!second) return dh::unexpected{second.error()};
        return first->VolumeSerialNumber == second->VolumeSerialNumber &&
            dh::memcmp(first->FileId.Identifier, second->FileId.Identifier, sizeof(first->FileId.Identifier)) == 0;
    }

    dh::expected<void, fs_failure> create_directories(const path& p) noexcept {
        if (p.empty()) return {};

        const wchar_t* const chars = p.c_str();
        const dh::size_t n = p.size();
        const dh::size_t root = root_length(chars, n);
        if (root == n) {
            const auto existing = status(p);
            if (!existing) return dh::unexpected{existing.error()};
            if (!existing->exists) return dh::unexpected{failure_from(ERROR_PATH_NOT_FOUND)};
            if (!existing->is_directory) return dh::unexpected{failure_from(ERROR_DIRECTORY)};
            return {};
        }

        dh::vector<wchar_t> prefix;
        prefix.resize(n + 1);
        for (dh::size_t i = 0; i < n; ++i)
            prefix[i] = chars[i];
        prefix[n] = L'\0';

        for (dh::size_t i = root; i <= n; ++i) {
            const bool at_end = (i == n);
            if (!at_end && chars[i] != k_sep) continue;

            if (i == 0 || chars[i - 1] == k_sep) continue;

            prefix[i] = L'\0';
            const bool made = ::CreateDirectoryW(prefix.data(), nullptr) != FALSE;
            const DWORD last_error = made ? DWORD{0} : ::GetLastError();
            if (!at_end) prefix[i] = chars[i];

            if (made) continue;
            if (last_error == ERROR_ALREADY_EXISTS) {
                const auto existing = status(path{chars, i});
                if (!existing) return dh::unexpected{existing.error()};
                if (existing->exists && existing->is_directory) continue;
                return dh::unexpected{fs_failure{fs_error::name_invalid, ERROR_DIRECTORY}};
            }
            return dh::unexpected<fs_failure>{failure_from(last_error)};
        }
        return {};
    }

    dh::expected<void, fs_failure> remove(const path& p) noexcept {
        auto st = status(p);
        if (!st) return dh::unexpected<fs_failure>{st.error()};
        if (!st->exists) {
            return dh::unexpected<fs_failure>{fs_failure{fs_error::not_found, static_cast<dh::u32>(ERROR_FILE_NOT_FOUND)}};
        }
        const BOOL ok = st->is_directory ? ::RemoveDirectoryW(p.c_str()) : ::DeleteFileW(p.c_str());
        if (!ok) {
            return dh::unexpected<fs_failure>{failure_from(::GetLastError())};
        }
        return {};
    }

    dh::expected<directory_iterator, fs_failure> directory_iterator::open(const path& dir) noexcept {
        directory_iterator it{};
        it.m_root = dir;

        path pattern = dir;
        pattern /= L"*";
        WIN32_FIND_DATAW fd{};
        HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) {
            const DWORD last_error = ::GetLastError();

            if (last_error == ERROR_FILE_NOT_FOUND) return dh::move(it);
            return dh::unexpected<fs_failure>{failure_from(last_error)};
        }
        it.m_handle = h;

        while (true) {
            if ((fd.cFileName[0] == L'.' && fd.cFileName[1] == L'\0') || (fd.cFileName[0] == L'.' && fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')) {
                if (!::FindNextFileW(static_cast<HANDLE>(it.m_handle), &fd)) {
                    const DWORD error = ::GetLastError();
                    ::FindClose(static_cast<HANDLE>(it.m_handle));
                    it.m_handle = nullptr;
                    if (error != ERROR_NO_MORE_FILES) return dh::unexpected{failure_from(error)};
                    return dh::move(it);
                }
                continue;
            }
            break;
        }
        it.load_current_from_find_data(&fd);
        return dh::move(it);
    }

    directory_iterator::directory_iterator(directory_iterator&& other) noexcept : m_handle(other.m_handle), m_root(static_cast<path&&>(other.m_root)), m_entry(static_cast<entry&&>(other.m_entry)) {
        other.m_handle = nullptr;
    }

    directory_iterator& directory_iterator::operator=(directory_iterator&& other) noexcept {
        if (this != &other) {
            if (m_handle) ::FindClose(static_cast<HANDLE>(m_handle));
            m_handle = other.m_handle;
            m_root = static_cast<path&&>(other.m_root);
            m_entry = static_cast<entry&&>(other.m_entry);
            other.m_handle = nullptr;
        }
        return *this;
    }

    directory_iterator::~directory_iterator() {
        if (m_handle) ::FindClose(static_cast<HANDLE>(m_handle));
    }

    dh::expected<void, fs_failure> directory_iterator::advance() noexcept {
        if (m_handle == nullptr) return {};
        WIN32_FIND_DATAW fd{};
        while (::FindNextFileW(static_cast<HANDLE>(m_handle), &fd)) {
            if ((fd.cFileName[0] == L'.' && fd.cFileName[1] == L'\0') || (fd.cFileName[0] == L'.' && fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')) {
                continue;
            }
            load_current_from_find_data(&fd);
            return {};
        }
        const DWORD error = ::GetLastError();
        ::FindClose(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
        if (error != ERROR_NO_MORE_FILES) return dh::unexpected{failure_from(error)};
        return {};
    }

    void directory_iterator::load_current_from_find_data(const void* find_data) noexcept {
        const auto& fd = *static_cast<const WIN32_FIND_DATAW*>(find_data);
        entry e{};
        e.file_path = m_root;
        e.file_path /= fd.cFileName;
        e.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        e.is_reparse_point = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        e.size_bytes = (static_cast<dh::u64>(fd.nFileSizeHigh) << 32) | static_cast<dh::u64>(fd.nFileSizeLow);
        e.last_write_filetime = filetime_to_u64(fd.ftLastWriteTime);
        m_entry = static_cast<entry&&>(e);
    }

    dh::expected<directory_watch, fs_failure> directory_watch::arm(const path& dir, watch_filter filter, watch_scope scope) noexcept {
        const DWORD names = FILE_NOTIFY_CHANGE_FILE_NAME | (scope == watch_scope::subtree ? FILE_NOTIFY_CHANGE_DIR_NAME : 0);
        const DWORD native_filter = filter == watch_filter::names ? names : names | FILE_NOTIFY_CHANGE_LAST_WRITE;
        const HANDLE h = ::FindFirstChangeNotificationW(dir.c_str(), scope == watch_scope::subtree ? TRUE : FALSE, native_filter);
        if (h == INVALID_HANDLE_VALUE) {
            return dh::unexpected<fs_failure>{failure_from(::GetLastError())};
        }
        directory_watch watch{};
        watch.m_handle = h;
        return dh::move(watch);
    }

    directory_watch::directory_watch(directory_watch&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    directory_watch& directory_watch::operator=(directory_watch&& other) noexcept {
        if (this != &other) {
            if (m_handle) ::FindCloseChangeNotification(static_cast<HANDLE>(m_handle));
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    directory_watch::~directory_watch() {
        if (m_handle) ::FindCloseChangeNotification(static_cast<HANDLE>(m_handle));
    }

    watch_state directory_watch::consume_change() noexcept {
        if (m_handle == nullptr) return watch_state::watch_lost;
        const DWORD result = ::WaitForSingleObject(static_cast<HANDLE>(m_handle), 0);
        if (result == WAIT_TIMEOUT) return watch_state::unchanged;
        if (result != WAIT_OBJECT_0) {
            ::FindCloseChangeNotification(static_cast<HANDLE>(m_handle));
            m_handle = nullptr;
            return watch_state::watch_lost;
        }
        if (::FindNextChangeNotification(static_cast<HANDLE>(m_handle)) == 0) {
            ::FindCloseChangeNotification(static_cast<HANDLE>(m_handle));
            m_handle = nullptr;
            return watch_state::watch_lost;
        }
        return watch_state::changed;
    }

}
