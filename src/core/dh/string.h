// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include <cstring>
#include "core/dh/type_traits.h"

namespace dh {

    template <typename CharT> [[nodiscard]] constexpr dh::size_t char_len(const CharT* s) noexcept {
        dh::size_t n = 0;
        if (s == nullptr) {
            return 0;
        }
        while (s[n] != CharT{}) {
            ++n;
        }
        return n;
    }

    [[nodiscard]] constexpr dh::size_t strlen(const char* s) noexcept {
        return char_len<char>(s);
    }

    [[nodiscard]] inline int strcmp(const char* a, const char* b) noexcept {
        return ::strcmp(a, b);
    }

    [[nodiscard]] constexpr char to_lower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    [[nodiscard]] constexpr char to_upper(char c) noexcept {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
    }

    inline constexpr dh::u32 k_first_printable_ascii = 0x20;
    inline constexpr dh::u32 k_last_printable_ascii = 0x7E;

    [[nodiscard]] constexpr bool is_printable_ascii(dh::u32 code_point) noexcept {
        return code_point >= k_first_printable_ascii && code_point <= k_last_printable_ascii;
    }

    template <typename CharT> class basic_string;

    template <typename CharT> class basic_string_view final {
    public:
        using value_type = CharT;
        using size_type = dh::size_t;

        static constexpr size_type npos = size_type(-1);

        constexpr basic_string_view() noexcept = default;
        constexpr basic_string_view(const basic_string_view&) noexcept = default;
        constexpr basic_string_view(basic_string_view&&) noexcept = default;
        constexpr basic_string_view& operator=(const basic_string_view&) noexcept = default;
        constexpr basic_string_view& operator=(basic_string_view&&) noexcept = default;
        ~basic_string_view() = default;

        constexpr basic_string_view(const CharT* data, size_type n) noexcept : m_data(data), m_size(n) {}

        constexpr basic_string_view(const CharT* cstr) noexcept : m_data(cstr), m_size(dh::char_len<CharT>(cstr)) {}

        [[nodiscard]] constexpr const CharT* data() const noexcept { return m_data; }

        [[nodiscard]] constexpr size_type size() const noexcept { return m_size; }

        [[nodiscard]] constexpr size_type length() const noexcept { return m_size; }

        [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

        constexpr const CharT& operator[](size_type i) const noexcept { return m_data[i]; }

        constexpr const CharT* begin() const noexcept { return m_data; }

        constexpr const CharT* end() const noexcept { return m_data + m_size; }

        [[nodiscard]] constexpr basic_string_view substr(size_type pos, size_type count = npos) const noexcept {
            if (pos > m_size) {
                return basic_string_view{m_data + m_size, 0};
            }
            const size_type remaining = m_size - pos;
            const size_type take = count < remaining ? count : remaining;
            return basic_string_view{m_data + pos, take};
        }

        constexpr void remove_prefix(size_type n) noexcept {
            const size_type take = n < m_size ? n : m_size;
            m_data += take;
            m_size -= take;
        }

        [[nodiscard]] constexpr int compare(basic_string_view other) const noexcept {
            using uchar = dh::make_unsigned_t<CharT>;
            const size_type a_n = m_size;
            const size_type b_n = other.m_size;

            const size_type prefix_len = a_n < b_n ? a_n : b_n;
            for (size_type i = 0; i < prefix_len; ++i) {
                const uchar ca = static_cast<uchar>(m_data[i]);
                const uchar cb = static_cast<uchar>(other.m_data[i]);
                if (ca != cb) {
                    return ca < cb ? -1 : 1;
                }
            }
            if (a_n == b_n) return 0;
            return a_n < b_n ? -1 : 1;
        }

        [[nodiscard]] constexpr bool starts_with(basic_string_view prefix) const noexcept {
            if (prefix.m_size > m_size) return false;
            return substr(0, prefix.m_size).compare(prefix) == 0;
        }

        [[nodiscard]] constexpr bool ends_with(basic_string_view suffix) const noexcept {
            if (suffix.m_size > m_size) return false;
            return substr(m_size - suffix.m_size, suffix.m_size).compare(suffix) == 0;
        }

        [[nodiscard]] constexpr size_type find(CharT c, size_type pos = 0) const noexcept {
            for (size_type i = pos; i < m_size; ++i) {
                if (m_data[i] == c) return i;
            }
            return npos;
        }

        [[nodiscard]] constexpr size_type find(basic_string_view needle, size_type pos = 0) const noexcept {
            if (needle.m_size == 0) {
                return pos <= m_size ? pos : npos;
            }

            if (needle.m_size > m_size) return npos;
            const size_type last = m_size - needle.m_size;
            for (size_type i = pos; i <= last; ++i) {
                size_type j = 0;
                while (j < needle.m_size && m_data[i + j] == needle.m_data[j]) {
                    ++j;
                }
                if (j == needle.m_size) return i;
            }
            return npos;
        }

        constexpr friend bool operator==(basic_string_view a, basic_string_view b) noexcept { return a.compare(b) == 0; }

        constexpr friend bool operator<(basic_string_view a, basic_string_view b) noexcept { return a.compare(b) < 0; }

    private:
        const CharT* m_data{nullptr};
        size_type m_size{0};
    };

    using string_view = basic_string_view<char>;

    [[nodiscard]] constexpr bool ascii_iequals(string_view a, string_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (dh::size_t index = 0; index < a.size(); ++index) {
            if (to_lower(a[index]) != to_lower(b[index])) return false;
        }
        return true;
    }

    using wstring_view = basic_string_view<wchar_t>;

    template <typename CharT> class basic_string final {
    public:
        using size_type = dh::size_t;
        using value_type = CharT;
        using pointer = CharT*;
        using const_pointer = const CharT*;
        using iterator = CharT*;
        using const_iterator = const CharT*;
        using view_type = basic_string_view<CharT>;

        static constexpr size_type k_sso_bytes = 16;
        static constexpr size_type k_sso_elems = k_sso_bytes / sizeof(CharT);
        static constexpr size_type k_sso_capacity = k_sso_elems - 1;

        static_assert(k_sso_elems >= 2, "sso buffer needs room for a character and its terminator");

        basic_string() noexcept;
        basic_string(const basic_string& other) noexcept;
        basic_string(basic_string&& other) noexcept;
        basic_string(const CharT* cstr) noexcept;
        basic_string(const CharT* data, size_type count) noexcept;
        basic_string(size_type count, CharT ch) noexcept;
        explicit basic_string(view_type sv) noexcept;
        ~basic_string();

        basic_string& operator=(const basic_string& other) noexcept;
        basic_string& operator=(basic_string&& other) noexcept;
        basic_string& operator=(const CharT* cstr) noexcept;

        [[nodiscard]] const CharT* c_str() const noexcept;
        [[nodiscard]] const CharT* data() const noexcept;
        [[nodiscard]] CharT* data() noexcept;
        [[nodiscard]] size_type size() const noexcept;
        [[nodiscard]] size_type length() const noexcept;
        [[nodiscard]] size_type capacity() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        CharT& operator[](size_type i) noexcept;
        const CharT& operator[](size_type i) const noexcept;
        CharT& front() noexcept;
        CharT& back() noexcept;
        const CharT& front() const noexcept;
        const CharT& back() const noexcept;

        iterator begin() noexcept;
        iterator end() noexcept;
        const_iterator begin() const noexcept;
        const_iterator end() const noexcept;

        void clear() noexcept;
        void push_back(CharT c) noexcept;
        void pop_back() noexcept;
        void resize(size_type new_size, CharT fill = CharT{}) noexcept;
        void reserve(size_type new_cap) noexcept;
        void shrink_to_fit() noexcept;

        basic_string& append(const CharT* s) noexcept;
        basic_string& append(const CharT* s, size_type n) noexcept;
        basic_string& append(const basic_string& s) noexcept;
        basic_string& append(view_type sv) noexcept;
        basic_string& append(size_type n, CharT c) noexcept;

        basic_string& operator+=(const CharT* s) noexcept { return append(s); }

        basic_string& operator+=(const basic_string& s) noexcept { return append(s); }

        basic_string& operator+=(view_type sv) noexcept { return append(sv); }

        basic_string& operator+=(CharT c) noexcept {
            push_back(c);
            return *this;
        }

        void swap(basic_string& other) noexcept;

        [[nodiscard]] int compare(const basic_string& other) const noexcept;
        [[nodiscard]] int compare(const CharT* cstr) const noexcept;
        [[nodiscard]] int compare(view_type sv) const noexcept;

        operator view_type() const noexcept;

    private:
        static constexpr dh::u8 k_heap_flag = 0x80;
        static constexpr size_type k_heap_capacity_flag = size_type{1} << (sizeof(size_type) * 8 - 1);

        static constexpr dh::u8 k_sso_size_mask = 0x0F;

        static_assert(k_heap_flag == static_cast<dh::u8>(k_heap_capacity_flag >> (sizeof(size_type) * 8 - 8)), "string heap flags must use the same bit");
        static_assert(k_sso_capacity <= static_cast<size_type>(k_sso_size_mask), "sso capacity exceeds the length mask");

        struct heap_layout {
            CharT* ptr;
            size_type size;

            size_type capacity_with_flag;
        };

        struct sso_layout {

            CharT buf[k_sso_elems];
            dh::u8 pad[7];
            dh::u8 size_byte;
        };

        union storage {
            heap_layout heap;
            sso_layout sso;
        };

        storage m_storage;

        static_assert(sizeof(heap_layout) == 24, "heap_layout must be 24 bytes");
        static_assert(sizeof(sso_layout) == 24, "sso_layout must be 24 bytes");
        static_assert(sizeof(storage) == 24, "string storage must be 24 bytes");

        static constexpr size_type k_byte_capacity = static_cast<size_type>(-1) / sizeof(CharT) - 1;
        static constexpr size_type k_max_elements = k_byte_capacity < k_heap_capacity_flag ? k_byte_capacity : k_heap_capacity_flag - 1;
        [[nodiscard]] bool is_heap() const noexcept;

        void set_sso_size(size_type n) noexcept;
        void set_heap(CharT* ptr, size_type size, size_type cap) noexcept;

        void grow_to(size_type new_cap) noexcept;

        [[nodiscard]] CharT* buffer() noexcept;
        [[nodiscard]] const CharT* buffer() const noexcept;

        void set_size(size_type n) noexcept;

        void init_empty() noexcept;

        void assign_elems(const CharT* src, size_type n) noexcept;

        static void fill_elems(CharT* dst, CharT c, size_type n) noexcept;
    };

    using string = basic_string<char>;
    using wstring = basic_string<wchar_t>;

    template <typename CharT> [[nodiscard]] bool operator==(const basic_string<CharT>& a, const basic_string<CharT>& b) noexcept {
        return a.compare(b) == 0;
    }

    template <typename CharT> [[nodiscard]] bool operator==(const basic_string<CharT>& a, const CharT* b) noexcept {
        return a.compare(b) == 0;
    }

    template <typename CharT> [[nodiscard]] bool operator==(const basic_string<CharT>& a, basic_string_view<CharT> b) noexcept {
        return a.compare(b) == 0;
    }

    template <typename CharT> [[nodiscard]] bool operator<(const basic_string<CharT>& a, const basic_string<CharT>& b) noexcept {
        return a.compare(b) < 0;
    }

    template <typename CharT> [[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT>& a, const basic_string<CharT>& b) noexcept {
        basic_string<CharT> out;
        out.reserve(a.size() + b.size());
        out.append(a);
        out.append(b);
        return out;
    }

    template <typename CharT> [[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT>& a, const CharT* b) noexcept {
        basic_string<CharT> out;
        const dh::size_t b_len = b != nullptr ? dh::char_len<CharT>(b) : 0;
        out.reserve(a.size() + b_len);
        out.append(a);
        if (b != nullptr) {
            out.append(b, b_len);
        }
        return out;
    }

    template <typename CharT> [[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT>& a, basic_string_view<CharT> b) noexcept {
        basic_string<CharT> out;
        out.reserve(a.size() + b.size());
        out.append(a);
        out.append(b);
        return out;
    }

    template <typename CharT> [[nodiscard]] basic_string<CharT> operator+(const CharT* a, const basic_string<CharT>& b) noexcept {
        basic_string<CharT> out;
        const dh::size_t a_len = a != nullptr ? dh::char_len<CharT>(a) : 0;
        out.reserve(a_len + b.size());
        if (a != nullptr) {
            out.append(a, a_len);
        }
        out.append(b);
        return out;
    }

}
