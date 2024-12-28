// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"

#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

#include <initializer_list>
#include <new>

namespace dh {

    template <typename T> class vector final {
    public:
        using value_type = T;
        using size_type = dh::size_t;
        using difference_type = dh::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;

        constexpr vector() noexcept = default;

        vector(const vector& other) noexcept {
            if (other.m_size == 0) {
                return;
            }
            grow_to(other.m_size);
            copy_construct_range(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }

        vector(vector&& other) noexcept : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }

        explicit vector(size_type count) noexcept {
            if (count == 0) {
                return;
            }
            grow_to(count);
            default_construct_range(m_data, count);
            m_size = count;
        }

        vector(size_type count, const T& fill) noexcept {
            if (count == 0) {
                return;
            }
            grow_to(count);
            fill_construct_range(m_data, count, fill);
            m_size = count;
        }

        template <typename Iter>
            requires(!dh::is_integral_v<Iter>)
        vector(Iter first, Iter last) noexcept {
            for (; first != last; ++first) {
                push_back(*first);
            }
        }

        vector(std::initializer_list<T> init) noexcept {
            if (init.size() == 0) {
                return;
            }
            grow_to(init.size());
            copy_construct_range(m_data, init.begin(), init.size());
            m_size = init.size();
        }

        ~vector() {
            destroy_range(m_data, m_size);
            dh::deallocate(m_data);
        }

        vector& operator=(const vector& other) noexcept {
            if (this == &other) {
                return *this;
            }
            destroy_range(m_data, m_size);
            m_size = 0;
            if (other.m_size > m_capacity) {
                dh::deallocate(m_data);
                m_data = nullptr;
                m_capacity = 0;
                grow_to(other.m_size);
            }
            copy_construct_range(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
            return *this;
        }

        vector& operator=(vector&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            destroy_range(m_data, m_size);
            dh::deallocate(m_data);
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
            return *this;
        }

        vector& operator=(std::initializer_list<T> init) noexcept {
            destroy_range(m_data, m_size);
            m_size = 0;
            if (init.size() > m_capacity) {
                dh::deallocate(m_data);
                m_data = nullptr;
                m_capacity = 0;
                grow_to(init.size());
            }
            copy_construct_range(m_data, init.begin(), init.size());
            m_size = init.size();
            return *this;
        }

        void assign(size_type count, const T& fill) noexcept {
            if (count == 0) {
                clear();
                return;
            }
            const T stable_fill(fill);
            destroy_range(m_data, m_size);
            m_size = 0;
            if (count > m_capacity) {
                dh::deallocate(m_data);
                m_data = nullptr;
                m_capacity = 0;
                grow_to(count);
            }
            fill_construct_range(m_data, count, stable_fill);
            m_size = count;
        }

        template <typename Iter>
            requires(!dh::is_integral_v<Iter>)
        void assign(Iter first, Iter last) noexcept {
            clear();
            for (; first != last; ++first) {
                push_back(*first);
            }
        }

        [[nodiscard]] reference operator[](size_type i) noexcept { return m_data[i]; }

        [[nodiscard]] const_reference operator[](size_type i) const noexcept { return m_data[i]; }

        [[nodiscard]] reference front() noexcept { return m_data[0]; }

        [[nodiscard]] reference back() noexcept { return m_data[m_size - 1]; }

        [[nodiscard]] const_reference front() const noexcept { return m_data[0]; }

        [[nodiscard]] const_reference back() const noexcept { return m_data[m_size - 1]; }

        [[nodiscard]] pointer data() noexcept { return m_data; }

        [[nodiscard]] const_pointer data() const noexcept { return m_data; }

        iterator begin() noexcept { return m_data; }

        iterator end() noexcept { return m_data + m_size; }

        const_iterator begin() const noexcept { return m_data; }

        const_iterator end() const noexcept { return m_data + m_size; }

        const_iterator cbegin() const noexcept { return m_data; }

        const_iterator cend() const noexcept { return m_data + m_size; }

        [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

        [[nodiscard]] size_type size() const noexcept { return m_size; }

        [[nodiscard]] size_type capacity() const noexcept { return m_capacity; }

        void reserve(size_type new_capacity) noexcept {
            if (new_capacity > m_capacity) {
                grow_to(new_capacity);
            }
        }

        void shrink_to_fit() noexcept {
            if (m_size == m_capacity) {
                return;
            }
            if (m_size == 0) {
                dh::deallocate(m_data);
                m_data = nullptr;
                m_capacity = 0;
                return;
            }
            T* fresh = static_cast<T*>(dh::allocate(m_size * sizeof(T), k_alignment));
            if (fresh == nullptr) {
                return;
            }
            relocate_range(fresh, m_data, m_size);
            dh::deallocate(m_data);
            m_data = fresh;
            m_capacity = m_size;
        }

        void clear() noexcept {
            destroy_range(m_data, m_size);
            m_size = 0;
        }

        void push_back(const T& value) noexcept { emplace_back(value); }

        void push_back(T&& value) noexcept { emplace_back(dh::move(value)); }

        template <typename... Args> T& emplace_back(Args&&... args) noexcept {
            if (m_size == k_max_elements) dh::alloc_failed("dh::vector::emplace_back: size overflow");
            if (m_size == m_capacity) {
                const size_type target = growth_capacity(m_size + 1);
                T* fresh = allocate_storage(target);

                // args can point into this vector, build the new item before moving the old ones.
                ::new (static_cast<void*>(fresh + m_size)) T(dh::forward<Args>(args)...);
                relocate_range(fresh, m_data, m_size);
                dh::deallocate(m_data);
                m_data = fresh;
                m_capacity = target;
            } else {
                ::new (static_cast<void*>(m_data + m_size)) T(dh::forward<Args>(args)...);
            }
            return m_data[m_size++];
        }

        void pop_back() noexcept {
            --m_size;
            m_data[m_size].~T();
        }

        void resize(size_type new_size) noexcept {
            if (new_size < m_size) {
                destroy_range(m_data + new_size, m_size - new_size);
                m_size = new_size;
                return;
            }
            if (new_size > m_capacity) {
                grow_to(new_size);
            }
            default_construct_range(m_data + m_size, new_size - m_size);
            m_size = new_size;
        }

        void resize(size_type new_size, const T& fill) noexcept {
            if (new_size < m_size) {
                destroy_range(m_data + new_size, m_size - new_size);
                m_size = new_size;
                return;
            }
            if (new_size > m_capacity) {
                const T stable_fill(fill);
                grow_to(new_size);
                fill_construct_range(m_data + m_size, new_size - m_size, stable_fill);
                m_size = new_size;
                return;
            }
            fill_construct_range(m_data + m_size, new_size - m_size, fill);
            m_size = new_size;
        }

        iterator insert(const_iterator pos, const T& value) noexcept {
            const size_type offset = static_cast<size_type>(pos - m_data);
            T stable_value(dh::forward<decltype(value)>(value));
            ensure_capacity_for_one();
            shift_right_by_one(offset);
            ::new (static_cast<void*>(m_data + offset)) T(dh::move(stable_value));
            ++m_size;
            return m_data + offset;
        }

        iterator insert(const_iterator pos, T&& value) noexcept {
            const size_type offset = static_cast<size_type>(pos - m_data);
            T stable_value(dh::forward<decltype(value)>(value));
            ensure_capacity_for_one();
            shift_right_by_one(offset);
            ::new (static_cast<void*>(m_data + offset)) T(dh::move(stable_value));
            ++m_size;
            return m_data + offset;
        }

        template <typename InputIt>
            requires(!dh::is_integral_v<InputIt>)
        iterator insert(const_iterator pos, InputIt first, InputIt last) noexcept {
            const size_type offset = static_cast<size_type>(pos - m_data);
            const auto count = static_cast<size_type>(last - first);
            if (count == 0) {
                return m_data + offset;
            }
            if (count > k_max_elements - m_size) dh::alloc_failed("dh::vector::insert: size overflow");
            if (m_size + count > m_capacity) {
                grow_to(m_size + count);
            }

            for (size_type i = m_size; i > offset; --i) {
                const size_type src = i - 1;
                ::new (static_cast<void*>(m_data + src + count)) T(dh::move(m_data[src]));
                m_data[src].~T();
            }
            size_type w = offset;
            for (InputIt it = first; it != last; ++it, ++w) {
                ::new (static_cast<void*>(m_data + w)) T(*it);
            }
            m_size += count;
            return m_data + offset;
        }

        iterator erase(const_iterator pos) noexcept {
            const size_type offset = static_cast<size_type>(pos - m_data);
            shift_left_by_one(offset);
            --m_size;
            return m_data + offset;
        }

        iterator erase(const_iterator first, const_iterator last) noexcept {
            const size_type start = static_cast<size_type>(first - m_data);
            const size_type stop = static_cast<size_type>(last - m_data);
            if (start == stop) {
                return m_data + start;
            }
            const size_type span = stop - start;
            shift_left_by(start, span);
            m_size -= span;
            return m_data + start;
        }

        void swap(vector& other) noexcept {
            T* const old_data = m_data;
            const size_type old_size = m_size;
            const size_type old_capacity = m_capacity;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = old_data;
            other.m_size = old_size;
            other.m_capacity = old_capacity;
        }

        [[nodiscard]] friend bool operator==(const vector& a, const vector& b) noexcept {
            if (a.m_size != b.m_size) {
                return false;
            }
            for (size_type i = 0; i < a.m_size; ++i) {
                if (!(a.m_data[i] == b.m_data[i])) {
                    return false;
                }
            }
            return true;
        }

    private:
        T* m_data{nullptr};
        size_type m_size{0};
        size_type m_capacity{0};

        static constexpr size_type k_size_type_max = static_cast<size_type>(-1);

        static constexpr size_type k_max_elements = k_size_type_max / sizeof(T);
        static constexpr size_type k_alignment = alignof(T) < alignof(void*) ? alignof(void*) : alignof(T);

        [[nodiscard]] size_type growth_capacity(size_type new_capacity) const noexcept {
            if (new_capacity > k_max_elements) {
                dh::alloc_failed("dh::vector::grow_to: size overflow");
            }
            size_type target = m_capacity == 0 ? size_type{4} : m_capacity;
            while (target < new_capacity) {
                if (target > k_max_elements / 2) {
                    target = new_capacity;
                    break;
                }
                target *= 2;
            }
            return target;
        }

        [[nodiscard]] static T* allocate_storage(size_type count) noexcept {
            T* fresh = static_cast<T*>(dh::allocate(count * sizeof(T), k_alignment));
            if (fresh == nullptr) dh::alloc_failed("dh::vector::allocate_storage");
            return fresh;
        }

        void grow_to(size_type new_capacity) noexcept {
            const size_type target = growth_capacity(new_capacity);
            T* fresh = allocate_storage(target);
            relocate_range(fresh, m_data, m_size);
            dh::deallocate(m_data);
            m_data = fresh;
            m_capacity = target;
        }

        void ensure_capacity_for_one() noexcept {
            if (m_size == k_max_elements) dh::alloc_failed("dh::vector: size overflow");
            if (m_size == m_capacity) {
                grow_to(m_size + 1);
            }
        }

        static void relocate_range(T* dst, T* src, size_type count) noexcept {
            if (count == 0) {
                return;
            }
            if constexpr (dh::is_trivially_copyable_v<T>) {
                dh::memcpy(dst, src, count * sizeof(T));
            } else {
                for (size_type i = 0; i < count; ++i) {
                    ::new (static_cast<void*>(dst + i)) T(dh::move(src[i]));
                    src[i].~T();
                }
            }
        }

        static void copy_construct_range(T* dst, const T* src, size_type count) noexcept {
            if constexpr (dh::is_trivially_copyable_v<T>) {
                if (count != 0) {
                    dh::memcpy(dst, src, count * sizeof(T));
                }
            } else {
                for (size_type i = 0; i < count; ++i) {
                    ::new (static_cast<void*>(dst + i)) T(src[i]);
                }
            }
        }

        static void default_construct_range(T* dst, size_type count) noexcept {
            if constexpr (dh::is_trivially_default_constructible_v<T>) {
                if (count != 0) {
                    dh::memset(dst, 0, count * sizeof(T));
                }
            } else {
                for (size_type i = 0; i < count; ++i) {
                    ::new (static_cast<void*>(dst + i)) T();
                }
            }
        }

        static void fill_construct_range(T* dst, size_type count, const T& fill) noexcept {
            for (size_type i = 0; i < count; ++i) {
                ::new (static_cast<void*>(dst + i)) T(fill);
            }
        }

        static void destroy_range(T* data, size_type count) noexcept {
            if constexpr (dh::is_trivially_destructible_v<T>) {
                (void)data;
                (void)count;
            } else {
                for (size_type i = 0; i < count; ++i) {
                    data[i].~T();
                }
            }
        }

        void shift_right_by_one(size_type offset) noexcept {
            const size_type tail = m_size - offset;
            if (tail == 0) {
                return;
            }
            if constexpr (dh::is_trivially_copyable_v<T>) {
                dh::memmove(m_data + offset + 1, m_data + offset, tail * sizeof(T));
            } else {
                ::new (static_cast<void*>(m_data + m_size)) T(dh::move(m_data[m_size - 1]));
                for (size_type i = m_size - 1; i > offset; --i) {
                    m_data[i] = dh::move(m_data[i - 1]);
                }
                m_data[offset].~T();
            }
        }

        void shift_left_by_one(size_type offset) noexcept {
            const size_type tail = m_size - offset - 1;
            if constexpr (dh::is_trivially_copyable_v<T>) {
                if (tail != 0) {
                    dh::memmove(m_data + offset, m_data + offset + 1, tail * sizeof(T));
                }
            } else {
                for (size_type i = offset; i < m_size - 1; ++i) {
                    m_data[i] = dh::move(m_data[i + 1]);
                }
                m_data[m_size - 1].~T();
            }
        }

        void shift_left_by(size_type offset, size_type span) noexcept {
            const size_type tail = m_size - offset - span;
            if constexpr (dh::is_trivially_copyable_v<T>) {
                if (tail != 0) {
                    dh::memmove(m_data + offset, m_data + offset + span, tail * sizeof(T));
                }
            } else {
                for (size_type i = 0; i < tail; ++i) {
                    m_data[offset + i] = dh::move(m_data[offset + span + i]);
                }
                for (size_type i = 0; i < span; ++i) {
                    m_data[m_size - 1 - i].~T();
                }
            }
        }
    };

}
