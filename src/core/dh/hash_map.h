// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"
#include "string.h"
#include "vector.h"

#include <new>
#include "core/dh/fnv1a.h"
#include "core/dh/memory.h"
#include "core/dh/type_traits.h"

namespace dh {

    template <typename T> struct hash;

    namespace detail {

        [[nodiscard]] inline u64 fnv1a_bytes(const void* bytes, size_t length) noexcept {
            const auto* cursor = static_cast<const unsigned char*>(bytes);
            u64 state = k_fnv1a64_offset_basis;
            for (size_t i = 0; i < length; ++i) {
                state ^= static_cast<u64>(cursor[i]);
                state *= k_fnv1a64_prime;
            }
            return state;
        }

        [[nodiscard]] inline constexpr u64 mix_integer(u64 value) noexcept {
            value ^= value >> 30;
            value *= 0xBF58476D1CE4E5B9ULL;
            value ^= value >> 27;
            value *= 0x94D049BB133111EBULL;
            value ^= value >> 31;
            return value;
        }

        template <typename T> union slot_storage {
            constexpr slot_storage() noexcept : unoccupied() {}

            ~slot_storage() noexcept {}

            slot_storage(const slot_storage&) = delete;
            slot_storage(slot_storage&&) = delete;
            slot_storage& operator=(const slot_storage&) = delete;
            slot_storage& operator=(slot_storage&&) = delete;

            unsigned char unoccupied;
            T object;
        };

    }

    template <> struct hash<u32> {
        [[nodiscard]] u64 operator()(u32 value) const noexcept { return detail::mix_integer(value); }
    };

    template <> struct hash<u64> {
        [[nodiscard]] u64 operator()(u64 value) const noexcept { return detail::mix_integer(value); }
    };

    template <> struct hash<i32> {
        [[nodiscard]] u64 operator()(i32 value) const noexcept { return detail::mix_integer(static_cast<u64>(static_cast<i64>(value))); }
    };

    template <> struct hash<i64> {
        [[nodiscard]] u64 operator()(i64 value) const noexcept { return detail::mix_integer(static_cast<u64>(value)); }
    };

    template <typename T> struct hash<T*> {
        [[nodiscard]] u64 operator()(T* value) const noexcept { return detail::mix_integer(reinterpret_cast<u64>(value)); }
    };

    template <> struct hash<string> {
        [[nodiscard]] u64 operator()(const string& value) const noexcept { return detail::fnv1a_bytes(value.data(), value.size()); }
    };

    template <> struct hash<string_view> {
        [[nodiscard]] u64 operator()(string_view value) const noexcept { return detail::fnv1a_bytes(value.data(), value.size()); }
    };

    template <typename T> struct hash {
        static_assert(dh::is_trivially_copyable_v<T> && (__has_unique_object_representations(T) || dh::is_floating_point_v<T>), "dh::hash needs unique representations or a custom hasher");

        [[nodiscard]] u64 operator()(const T& value) const noexcept {
            if constexpr (dh::is_floating_point_v<T>) {
                if (value == T{0}) return detail::mix_integer(0);
            }
            unsigned char scratch[sizeof(T)];
            dh::memcpy(scratch, &value, sizeof(T));
            return detail::fnv1a_bytes(scratch, sizeof(T));
        }
    };

    template <typename K, typename V, typename H = hash<K>> class hash_map final {
    public:
        using key_type = K;
        using mapped_type = V;
        using size_type = size_t;

        struct pair_type {
            const K& key;
            V& value;
        };

        struct const_pair_type {
            const K& key;
            const V& value;
        };

        static constexpr u64 k_empty_hash = 0;

        static constexpr size_type k_load_numerator = 7;
        static constexpr size_type k_load_denominator = 8;

        static constexpr size_type k_initial_capacity = 16;

        hash_map() noexcept = default;

        explicit hash_map(size_type initial_capacity) noexcept {
            if (initial_capacity > 0) {
                reserve(initial_capacity);
            }
        }

        hash_map(const hash_map& other) noexcept : m_size(0), m_hasher(other.m_hasher) { copy_from(other); }

        hash_map(hash_map&& other) noexcept : m_entries(static_cast<vector<entry>&&>(other.m_entries)), m_size(other.m_size), m_hasher(dh::move(other.m_hasher)) {
            other.m_size = 0;
        }

        ~hash_map() = default;

        hash_map& operator=(const hash_map& other) noexcept {
            if (this != &other) {
                clear();
                m_hasher = other.m_hasher;
                copy_from(other);
            }
            return *this;
        }

        hash_map& operator=(hash_map&& other) noexcept {
            if (this != &other) {
                clear();
                m_entries = static_cast<vector<entry>&&>(other.m_entries);
                m_size = other.m_size;
                m_hasher = dh::move(other.m_hasher);
                other.m_size = 0;
            }
            return *this;
        }

        [[nodiscard]] size_type size() const noexcept { return m_size; }

        [[nodiscard]] size_type capacity() const noexcept { return m_entries.size(); }

        [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

        void clear() noexcept {
            for (size_type i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].hash != k_empty_hash) {
                    m_entries[i].clear_slot();
                }
            }
            m_size = 0;
        }

        void reserve(size_type new_cap) noexcept {
            size_type rounded = k_initial_capacity;
            constexpr size_type maximum = static_cast<size_type>(-1) / sizeof(entry);
            while (rounded < new_cap) {
                if (rounded > maximum / 2) dh::alloc_failed("dh::hash_map::reserve: size overflow");
                rounded <<= 1;
            }
            if (rounded <= m_entries.size()) {
                return;
            }
            rehash_to(rounded);
        }

        V& operator[](const K& key) noexcept {
            auto result = emplace(key);
            return *result.slot;
        }

        V& operator[](K&& key) noexcept {
            auto result = emplace(static_cast<K&&>(key));
            return *result.slot;
        }

        [[nodiscard]] V* find(const K& key) noexcept {
            const auto idx = locate(key);
            return idx == k_not_found ? nullptr : &m_entries[idx].value();
        }

        [[nodiscard]] const V* find(const K& key) const noexcept {
            const auto idx = locate(key);
            return idx == k_not_found ? nullptr : &m_entries[idx].value();
        }

        struct emplace_result {
            V* slot;
            bool inserted;
        };

        template <typename... Args> emplace_result emplace(const K& key, Args&&... args) noexcept {
            K key_copy{key};
            return emplace_impl(static_cast<K&&>(key_copy), static_cast<Args&&>(args)...);
        }

        template <typename... Args> emplace_result emplace(K&& key, Args&&... args) noexcept {
            return emplace_impl(static_cast<K&&>(key), static_cast<Args&&>(args)...);
        }

        bool erase(const K& key) noexcept {
            if (m_entries.size() == 0) {
                return false;
            }
            const u64 hash_value = compute_hash(key);
            const size_type mask = m_entries.size() - 1;
            size_type bucket = static_cast<size_type>(hash_value) & mask;
            size_type distance = 0;

            for (;;) {
                entry& slot = m_entries[bucket];
                if (slot.hash == k_empty_hash) {
                    return false;
                }
                if (slot.probe_distance < distance) {
                    return false;
                }
                if (slot.hash == hash_value && slot.key() == key) {
                    break;
                }
                bucket = (bucket + 1) & mask;
                ++distance;
                if (distance >= m_entries.size()) {
                    return false;
                }
            }

            m_entries[bucket].clear_slot();
            size_type prev = bucket;
            for (;;) {
                const size_type next = (prev + 1) & mask;
                entry& candidate = m_entries[next];
                if (candidate.hash == k_empty_hash || candidate.probe_distance == 0) {
                    break;
                }
                m_entries[prev].adopt(static_cast<entry&&>(candidate));
                m_entries[prev].probe_distance = candidate.probe_distance - 1;
                candidate.clear_slot();
                prev = next;
            }
            --m_size;
            return true;
        }

        class iterator final {
        public:
            iterator() noexcept = default;

            iterator(hash_map* parent, size_type index) noexcept : m_parent(parent), m_index(index) {}

            iterator(const iterator&) = default;
            iterator(iterator&&) noexcept = default;
            iterator& operator=(const iterator&) = default;
            iterator& operator=(iterator&&) noexcept = default;
            ~iterator() = default;

            [[nodiscard]] pair_type operator*() const noexcept {
                entry& slot = m_parent->m_entries[m_index];
                return pair_type{slot.key(), slot.value()};
            }

            iterator& operator++() noexcept {
                const size_type cap = m_parent->m_entries.size();
                ++m_index;
                while (m_index < cap && m_parent->m_entries[m_index].hash == k_empty_hash) {
                    ++m_index;
                }
                return *this;
            }

            [[nodiscard]] bool operator==(const iterator& other) const noexcept { return m_parent == other.m_parent && m_index == other.m_index; }

            [[nodiscard]] bool operator!=(const iterator& other) const noexcept { return !(*this == other); }

        private:
            hash_map* m_parent{nullptr};
            size_type m_index{0};
        };

        class const_iterator final {
        public:
            const_iterator() noexcept = default;

            const_iterator(const hash_map* parent, size_type index) noexcept : m_parent(parent), m_index(index) {}

            const_iterator(const const_iterator&) = default;
            const_iterator(const_iterator&&) noexcept = default;
            const_iterator& operator=(const const_iterator&) = default;
            const_iterator& operator=(const_iterator&&) noexcept = default;
            ~const_iterator() = default;

            [[nodiscard]] const_pair_type operator*() const noexcept {
                const entry& slot = m_parent->m_entries[m_index];
                return const_pair_type{slot.key(), slot.value()};
            }

            const_iterator& operator++() noexcept {
                const size_type cap = m_parent->m_entries.size();
                ++m_index;
                while (m_index < cap && m_parent->m_entries[m_index].hash == k_empty_hash) {
                    ++m_index;
                }
                return *this;
            }

            [[nodiscard]] bool operator==(const const_iterator& other) const noexcept { return m_parent == other.m_parent && m_index == other.m_index; }

            [[nodiscard]] bool operator!=(const const_iterator& other) const noexcept { return !(*this == other); }

        private:
            const hash_map* m_parent{nullptr};
            size_type m_index{0};
        };

        [[nodiscard]] iterator begin() noexcept {
            size_type idx = 0;
            const size_type cap = m_entries.size();
            while (idx < cap && m_entries[idx].hash == k_empty_hash) {
                ++idx;
            }
            return iterator{this, idx};
        }

        [[nodiscard]] iterator end() noexcept { return iterator{this, m_entries.size()}; }

        [[nodiscard]] const_iterator begin() const noexcept {
            size_type idx = 0;
            const size_type cap = m_entries.size();
            while (idx < cap && m_entries[idx].hash == k_empty_hash) {
                ++idx;
            }
            return const_iterator{this, idx};
        }

        [[nodiscard]] const_iterator end() const noexcept { return const_iterator{this, m_entries.size()}; }

    private:
        struct entry final {
            u64 hash{k_empty_hash};
            size_type probe_distance{0};
            detail::slot_storage<K> key_storage;
            detail::slot_storage<V> value_storage;

            entry() noexcept = default;

            ~entry() noexcept {
                if (hash != k_empty_hash) {
                    clear_slot();
                }
            }

            entry(const entry& other) noexcept : hash(other.hash), probe_distance(other.probe_distance) {
                if (hash != k_empty_hash) {
                    ::new (key_bytes()) K(other.key());
                    ::new (value_bytes()) V(other.value());
                }
            }

            entry(entry&& other) noexcept : hash(other.hash), probe_distance(other.probe_distance) {
                if (hash != k_empty_hash) {
                    ::new (key_bytes()) K(static_cast<K&&>(other.key()));
                    ::new (value_bytes()) V(static_cast<V&&>(other.value()));
                    other.clear_slot();
                }
            }

            entry& operator=(const entry& other) noexcept {
                if (this != &other) {
                    if (hash != k_empty_hash) {
                        clear_slot();
                    }
                    hash = other.hash;
                    probe_distance = other.probe_distance;
                    if (hash != k_empty_hash) {
                        ::new (key_bytes()) K(other.key());
                        ::new (value_bytes()) V(other.value());
                    }
                }
                return *this;
            }

            entry& operator=(entry&& other) noexcept {
                if (this != &other) {
                    if (hash != k_empty_hash) {
                        clear_slot();
                    }
                    hash = other.hash;
                    probe_distance = other.probe_distance;
                    if (hash != k_empty_hash) {
                        ::new (key_bytes()) K(static_cast<K&&>(other.key()));
                        ::new (value_bytes()) V(static_cast<V&&>(other.value()));
                        other.clear_slot();
                    }
                }
                return *this;
            }

            [[nodiscard]] void* key_bytes() noexcept { return &key_storage.object; }

            [[nodiscard]] void* value_bytes() noexcept { return &value_storage.object; }

            [[nodiscard]] K& key() noexcept { return *dh::launder(&key_storage.object); }

            [[nodiscard]] const K& key() const noexcept { return *dh::launder(&key_storage.object); }

            [[nodiscard]] V& value() noexcept { return *dh::launder(&value_storage.object); }

            [[nodiscard]] const V& value() const noexcept { return *dh::launder(&value_storage.object); }

            void adopt(entry&& source) noexcept {
                hash = source.hash;
                probe_distance = source.probe_distance;
                ::new (key_bytes()) K(static_cast<K&&>(source.key()));
                ::new (value_bytes()) V(static_cast<V&&>(source.value()));
            }

            template <typename KeyArg, typename... ValueArgs>
            void construct(u64 hash_value, size_type distance, KeyArg&& key_in, ValueArgs&&... value_args) noexcept {
                hash = hash_value;
                probe_distance = distance;
                ::new (key_bytes()) K(static_cast<KeyArg&&>(key_in));
                ::new (value_bytes()) V(static_cast<ValueArgs&&>(value_args)...);
            }

            void clear_slot() noexcept {
                key().~K();
                value().~V();
                hash = k_empty_hash;
                probe_distance = 0;
            }
        };

        static constexpr size_type k_not_found = static_cast<size_type>(-1);

        [[nodiscard]] u64 compute_hash(const K& key) const noexcept {
            const u64 raw = m_hasher(key);

            // zero marks an empty bucket, never store it as a real hash.
            return raw == k_empty_hash ? 1ULL : raw;
        }

        [[nodiscard]] size_type locate(const K& key) const noexcept {
            if (m_entries.size() == 0) {
                return k_not_found;
            }
            const u64 hash_value = compute_hash(key);
            const size_type mask = m_entries.size() - 1;
            size_type bucket = static_cast<size_type>(hash_value) & mask;
            size_type distance = 0;

            for (;;) {
                const entry& slot = m_entries[bucket];
                if (slot.hash == k_empty_hash) {
                    return k_not_found;
                }
                if (slot.probe_distance < distance) {
                    return k_not_found;
                }
                if (slot.hash == hash_value && slot.key() == key) {
                    return bucket;
                }
                bucket = (bucket + 1) & mask;
                ++distance;
                if (distance >= m_entries.size()) {
                    return k_not_found;
                }
            }
        }

        template <typename... Args> emplace_result emplace_impl(K&& key, Args&&... args) noexcept {
            const u64 hash_value = compute_hash(key);
            entry pending;
            pending.construct(hash_value, 0, static_cast<K&&>(key), static_cast<Args&&>(args)...);
            if (m_entries.size() == 0) {
                rehash_to(k_initial_capacity);
            } else if (load_exceeded(m_size + 1)) {
                rehash_to(m_entries.size() * 2);
            }

            const size_type mask = m_entries.size() - 1;
            size_type bucket = static_cast<size_type>(hash_value) & mask;
            size_type distance = 0;

            V* return_slot = nullptr;
            bool placed_pending = false;

            for (;;) {
                entry& slot = m_entries[bucket];
                if (slot.hash == k_empty_hash) {
                    slot.adopt(static_cast<entry&&>(pending));
                    slot.probe_distance = distance;
                    pending.clear_slot();

                    if (!placed_pending) {
                        return_slot = &slot.value();
                        ++m_size;
                    }
                    return emplace_result{return_slot, true};
                }
                if (!placed_pending && slot.hash == hash_value && slot.key() == pending.key()) {
                    pending.clear_slot();
                    return emplace_result{&slot.value(), false};
                }
                if (slot.probe_distance < distance) {
                    entry victim;
                    victim.adopt(static_cast<entry&&>(slot));
                    slot.clear_slot();
                    slot.adopt(static_cast<entry&&>(pending));
                    slot.probe_distance = distance;
                    if (!placed_pending) {
                        return_slot = &slot.value();
                        placed_pending = true;
                        ++m_size;
                    }
                    pending.clear_slot();
                    pending.adopt(static_cast<entry&&>(victim));
                    distance = pending.probe_distance;
                    victim.clear_slot();
                }
                bucket = (bucket + 1) & mask;
                ++distance;
                pending.probe_distance = distance;
            }
        }

        [[nodiscard]] bool load_exceeded(size_type next_size) const noexcept {
            return next_size > (m_entries.size() / k_load_denominator) * k_load_numerator;
        }

        void rehash_to(size_type new_cap) noexcept {
            vector<entry> old_entries(static_cast<vector<entry>&&>(m_entries));
            m_entries = vector<entry>{};
            m_entries.resize(new_cap);
            m_size = 0;

            for (size_type i = 0; i < old_entries.size(); ++i) {
                entry& slot = old_entries[i];
                if (slot.hash == k_empty_hash) {
                    continue;
                }
                reinsert_for_rehash(static_cast<entry&&>(slot));
                slot.clear_slot();
            }
        }

        void reinsert_for_rehash(entry&& moved) noexcept {
            const u64 hash_value = moved.hash;
            const size_type mask = m_entries.size() - 1;
            size_type bucket = static_cast<size_type>(hash_value) & mask;
            size_type distance = 0;

            entry pending;
            pending.adopt(static_cast<entry&&>(moved));
            pending.probe_distance = 0;

            for (;;) {
                entry& slot = m_entries[bucket];
                if (slot.hash == k_empty_hash) {
                    slot.adopt(static_cast<entry&&>(pending));
                    slot.probe_distance = distance;
                    pending.clear_slot();
                    ++m_size;
                    return;
                }
                if (slot.probe_distance < distance) {
                    entry victim;
                    victim.adopt(static_cast<entry&&>(slot));
                    slot.clear_slot();
                    slot.adopt(static_cast<entry&&>(pending));
                    slot.probe_distance = distance;
                    pending.clear_slot();
                    pending.adopt(static_cast<entry&&>(victim));
                    distance = pending.probe_distance;
                    victim.clear_slot();
                }
                bucket = (bucket + 1) & mask;
                ++distance;
                pending.probe_distance = distance;
            }
        }

        void copy_from(const hash_map& other) noexcept {
            if (other.m_size == 0) {
                return;
            }
            m_entries.resize(other.m_entries.size());
            for (size_type i = 0; i < other.m_entries.size(); ++i) {
                const entry& src = other.m_entries[i];
                if (src.hash == k_empty_hash) {
                    continue;
                }
                entry& dst = m_entries[i];
                dst.hash = src.hash;
                dst.probe_distance = src.probe_distance;
                ::new (dst.key_bytes()) K(src.key());
                ::new (dst.value_bytes()) V(src.value());
            }
            m_size = other.m_size;
        }

        vector<entry> m_entries{};
        size_type m_size{0};
        [[msvc::no_unique_address]] H m_hasher{};
    };

}
