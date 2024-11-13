// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "string.h"


namespace dh {

    namespace {

        constexpr dh::size_t k_min_heap_capacity = 16;

        [[nodiscard]] dh::size_t compute_growth(dh::size_t current, dh::size_t needed, dh::size_t max_elements) noexcept {
            dh::size_t cap = current < k_min_heap_capacity ? k_min_heap_capacity : current;
            while (cap < needed) {
                if (cap > max_elements / 2) {
                    cap = needed;
                    break;
                }
                cap *= 2;
            }
            return cap;
        }

    }

    template <typename CharT> void basic_string<CharT>::fill_elems(CharT* dst, CharT c, size_type n) noexcept {
        if constexpr (sizeof(CharT) == 1) {
            dh::memset(dst, static_cast<dh::u8>(c), n);
        } else {
            for (size_type i = 0; i < n; ++i) {
                dst[i] = c;
            }
        }
    }

    template <typename CharT> bool basic_string<CharT>::is_heap() const noexcept {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&m_storage);
        return (bytes[sizeof(m_storage) - 1] & k_heap_flag) != 0;
    }

    template <typename CharT> void basic_string<CharT>::init_empty() noexcept {
        m_storage.sso.buf[0] = CharT{};
        m_storage.sso.size_byte = 0;
    }

    template <typename CharT> void basic_string<CharT>::set_sso_size(size_type n) noexcept {
        m_storage.sso.size_byte = static_cast<dh::u8>(n & static_cast<size_type>(k_sso_size_mask));
    }

    template <typename CharT> void basic_string<CharT>::set_heap(CharT* ptr, size_type size, size_type cap) noexcept {
        m_storage.heap.ptr = ptr;
        m_storage.heap.size = size;

        m_storage.heap.capacity_with_flag = cap | k_heap_capacity_flag;
    }

    template <typename CharT> CharT* basic_string<CharT>::buffer() noexcept {
        return is_heap() ? m_storage.heap.ptr : m_storage.sso.buf;
    }

    template <typename CharT> const CharT* basic_string<CharT>::buffer() const noexcept {
        return is_heap() ? m_storage.heap.ptr : m_storage.sso.buf;
    }

    template <typename CharT> void basic_string<CharT>::set_size(size_type n) noexcept {
        if (is_heap()) {
            m_storage.heap.size = n;
        } else {
            set_sso_size(n);
        }
    }

    template <typename CharT> void basic_string<CharT>::grow_to(size_type new_cap) noexcept {
        if (new_cap > k_max_elements) {
            dh::alloc_failed("dh::string::grow_to: size overflow");
        }

        const size_type current_size = size();
        const size_type current_cap = capacity();
        const size_type target_cap = compute_growth(current_cap, new_cap, k_max_elements);

        CharT* fresh = static_cast<CharT*>(dh::allocate((target_cap + 1) * sizeof(CharT)));

        if (fresh == nullptr) {
            dh::alloc_failed("dh::basic_string::grow_to");
        }

        const CharT* src = buffer();
        if (current_size != 0) {
            dh::memcpy(fresh, src, current_size * sizeof(CharT));
        }
        fresh[current_size] = CharT{};

        if (is_heap()) {
            dh::deallocate(m_storage.heap.ptr);
        }

        set_heap(fresh, current_size, target_cap);
    }

    template <typename CharT> void basic_string<CharT>::assign_elems(const CharT* src, size_type n) noexcept {
        if (src == nullptr && n != 0) dh::fail_fast("dh::string: null source", FAST_FAIL_INVALID_ARG);
        const auto source = reinterpret_cast<dh::u64>(src);
        const auto base = reinterpret_cast<dh::u64>(buffer());
        const bool borrowed = source >= base && source - base <= size() * sizeof(CharT);
        const size_type offset = borrowed ? (source - base) / sizeof(CharT) : 0;
        if (n > capacity()) {
            grow_to(n);
        }
        CharT* dst = buffer();
        if (borrowed) src = dst + offset;
        if (n != 0) {
            dh::memmove(dst, src, n * sizeof(CharT));
        }
        dst[n] = CharT{};
        set_size(n);
    }

    template <typename CharT> basic_string<CharT>::basic_string() noexcept {
        init_empty();
    }

    template <typename CharT> basic_string<CharT>::basic_string(const basic_string& other) noexcept {
        init_empty();
        assign_elems(other.buffer(), other.size());
    }

    template <typename CharT> basic_string<CharT>::basic_string(basic_string&& other) noexcept {
        m_storage = other.m_storage;
        other.init_empty();
    }

    template <typename CharT> basic_string<CharT>::basic_string(const CharT* cstr) noexcept {
        init_empty();
        if (cstr != nullptr) {
            assign_elems(cstr, dh::char_len<CharT>(cstr));
        }
    }

    template <typename CharT> basic_string<CharT>::basic_string(const CharT* data, size_type count) noexcept {
        init_empty();
        assign_elems(data, count);
    }

    template <typename CharT> basic_string<CharT>::basic_string(size_type count, CharT ch) noexcept {
        init_empty();
        if (count > capacity()) {
            grow_to(count);
        }
        CharT* dst = buffer();
        fill_elems(dst, ch, count);
        dst[count] = CharT{};
        set_size(count);
    }

    template <typename CharT> basic_string<CharT>::basic_string(view_type sv) noexcept {
        init_empty();
        assign_elems(sv.data(), sv.size());
    }

    template <typename CharT> basic_string<CharT>::~basic_string() {
        if (is_heap()) {
            dh::deallocate(m_storage.heap.ptr);
        }
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::operator=(const basic_string& other) noexcept {
        if (this == &other) {
            return *this;
        }
        assign_elems(other.buffer(), other.size());
        return *this;
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::operator=(basic_string&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (is_heap()) {
            dh::deallocate(m_storage.heap.ptr);
        }
        m_storage = other.m_storage;
        other.init_empty();
        return *this;
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::operator=(const CharT* cstr) noexcept {
        if (cstr == nullptr) {
            clear();
        } else {
            assign_elems(cstr, dh::char_len<CharT>(cstr));
        }
        return *this;
    }

    template <typename CharT> const CharT* basic_string<CharT>::c_str() const noexcept {
        return buffer();
    }

    template <typename CharT> const CharT* basic_string<CharT>::data() const noexcept {
        return buffer();
    }

    template <typename CharT> CharT* basic_string<CharT>::data() noexcept {
        return buffer();
    }

    template <typename CharT> dh::size_t basic_string<CharT>::size() const noexcept {
        return is_heap() ? m_storage.heap.size : static_cast<size_type>(m_storage.sso.size_byte & k_sso_size_mask);
    }

    template <typename CharT> dh::size_t basic_string<CharT>::length() const noexcept {
        return size();
    }

    template <typename CharT> dh::size_t basic_string<CharT>::capacity() const noexcept {
        if (is_heap()) {
            return m_storage.heap.capacity_with_flag & ~k_heap_capacity_flag;
        }
        return k_sso_capacity;
    }

    template <typename CharT> bool basic_string<CharT>::empty() const noexcept {
        return size() == 0;
    }

    template <typename CharT> CharT& basic_string<CharT>::operator[](size_type i) noexcept {
        return buffer()[i];
    }

    template <typename CharT> const CharT& basic_string<CharT>::operator[](size_type i) const noexcept {
        return buffer()[i];
    }

    template <typename CharT> CharT& basic_string<CharT>::front() noexcept {
        return buffer()[0];
    }

    template <typename CharT> CharT& basic_string<CharT>::back() noexcept {
        return buffer()[size() - 1];
    }

    template <typename CharT> const CharT& basic_string<CharT>::front() const noexcept {
        return buffer()[0];
    }

    template <typename CharT> const CharT& basic_string<CharT>::back() const noexcept {
        return buffer()[size() - 1];
    }

    template <typename CharT> typename basic_string<CharT>::iterator basic_string<CharT>::begin() noexcept {
        return buffer();
    }

    template <typename CharT> typename basic_string<CharT>::iterator basic_string<CharT>::end() noexcept {
        return buffer() + size();
    }

    template <typename CharT> typename basic_string<CharT>::const_iterator basic_string<CharT>::begin() const noexcept {
        return buffer();
    }

    template <typename CharT> typename basic_string<CharT>::const_iterator basic_string<CharT>::end() const noexcept {
        return buffer() + size();
    }

    template <typename CharT> void basic_string<CharT>::clear() noexcept {
        if (is_heap()) {
            m_storage.heap.ptr[0] = CharT{};
            m_storage.heap.size = 0;
        } else {
            m_storage.sso.buf[0] = CharT{};
            m_storage.sso.size_byte = 0;
        }
    }

    template <typename CharT> void basic_string<CharT>::push_back(CharT c) noexcept {
        const size_type old_size = size();
        if (old_size + 1 > capacity()) {
            grow_to(old_size + 1);
        }
        CharT* dst = buffer();
        dst[old_size] = c;
        dst[old_size + 1] = CharT{};
        set_size(old_size + 1);
    }

    template <typename CharT> void basic_string<CharT>::pop_back() noexcept {
        const size_type old_size = size();
        if (old_size == 0) {
            return;
        }
        CharT* dst = buffer();
        dst[old_size - 1] = CharT{};
        set_size(old_size - 1);
    }

    template <typename CharT> void basic_string<CharT>::resize(size_type new_size, CharT fill) noexcept {
        const size_type old_size = size();
        if (new_size > capacity()) {
            grow_to(new_size);
        }
        CharT* dst = buffer();
        if (new_size > old_size) {
            fill_elems(dst + old_size, fill, new_size - old_size);
        }
        dst[new_size] = CharT{};
        set_size(new_size);
    }

    template <typename CharT> void basic_string<CharT>::reserve(size_type new_cap) noexcept {
        if (new_cap <= capacity()) {
            return;
        }
        grow_to(new_cap);
    }

    template <typename CharT> void basic_string<CharT>::shrink_to_fit() noexcept {
        if (!is_heap()) {
            return;
        }
        const size_type current_size = m_storage.heap.size;

        if (current_size <= k_sso_capacity) {
            CharT* old_ptr = m_storage.heap.ptr;
            CharT copy[k_sso_elems];
            if (current_size != 0) {
                dh::memcpy(copy, old_ptr, current_size * sizeof(CharT));
            }
            copy[current_size] = CharT{};

            dh::deallocate(old_ptr);

            m_storage.sso.size_byte = 0;
            if (current_size != 0) {
                dh::memcpy(m_storage.sso.buf, copy, current_size * sizeof(CharT));
            }
            m_storage.sso.buf[current_size] = CharT{};
            set_sso_size(current_size);
            return;
        }

        const size_type current_cap = m_storage.heap.capacity_with_flag & ~k_heap_capacity_flag;
        if (current_size == current_cap) {
            return;
        }
        CharT* fresh = static_cast<CharT*>(dh::allocate((current_size + 1) * sizeof(CharT)));
        if (fresh == nullptr) {
            return;
        }
        dh::memcpy(fresh, m_storage.heap.ptr, current_size * sizeof(CharT));
        fresh[current_size] = CharT{};
        dh::deallocate(m_storage.heap.ptr);
        set_heap(fresh, current_size, current_size);
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::append(const CharT* s) noexcept {
        if (s == nullptr) {
            return *this;
        }
        return append(s, dh::char_len<CharT>(s));
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::append(const CharT* s, size_type n) noexcept {
        if (n == 0) return *this;
        if (s == nullptr) dh::fail_fast("dh::string::append: null source", FAST_FAIL_INVALID_ARG);
        const size_type old_size = size();
        if (n > k_max_elements - old_size) dh::alloc_failed("dh::string::append: size overflow");
        // copy first, s can point into this string.
        const basic_string<CharT> stable{s, n};
        const size_type new_size = old_size + n;
        if (new_size > capacity()) grow_to(new_size);
        CharT* dst = buffer();
        dh::memcpy(dst + old_size, stable.buffer(), n * sizeof(CharT));
        dst[new_size] = CharT{};
        set_size(new_size);
        return *this;
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::append(const basic_string& s) noexcept {
        return append(s.buffer(), s.size());
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::append(view_type sv) noexcept {
        return append(sv.data(), sv.size());
    }

    template <typename CharT> basic_string<CharT>& basic_string<CharT>::append(size_type n, CharT c) noexcept {
        if (n == 0) {
            return *this;
        }
        const size_type old_size = size();
        if (n > k_max_elements - old_size) dh::alloc_failed("dh::string::append: size overflow");
        const size_type new_size = old_size + n;
        if (new_size > capacity()) {
            grow_to(new_size);
        }
        CharT* dst = buffer();
        fill_elems(dst + old_size, c, n);
        dst[new_size] = CharT{};
        set_size(new_size);
        return *this;
    }

    template <typename CharT> void basic_string<CharT>::swap(basic_string& other) noexcept {
        const storage tmp = m_storage;
        m_storage = other.m_storage;
        other.m_storage = tmp;
    }

    template <typename CharT> int basic_string<CharT>::compare(const basic_string& other) const noexcept {
        return compare(view_type{other.buffer(), other.size()});
    }

    template <typename CharT> int basic_string<CharT>::compare(const CharT* cstr) const noexcept {
        if (cstr == nullptr) {
            return empty() ? 0 : 1;
        }
        return compare(view_type{cstr});
    }

    template <typename CharT> int basic_string<CharT>::compare(view_type sv) const noexcept {
        return view_type{buffer(), size()}.compare(sv);
    }

    template <typename CharT> basic_string<CharT>::operator view_type() const noexcept {
        return view_type{buffer(), size()};
    }

    template class basic_string<char>;
    template class basic_string<wchar_t>;

}
