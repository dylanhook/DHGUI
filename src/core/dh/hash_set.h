// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "hash_map.h"

namespace dh {

    struct monostate final {
        [[nodiscard]] friend bool operator==(monostate, monostate) noexcept { return true; }

        [[nodiscard]] friend bool operator!=(monostate, monostate) noexcept { return false; }
    };

    template <typename K, typename Hash = hash<K>> class hash_set final {
    public:
        using key_type = K;
        using value_type = K;
        using size_type = size_t;
        using hasher = Hash;

        hash_set() noexcept = default;

        explicit hash_set(size_type initial_capacity) noexcept : m_map(initial_capacity) {}

        hash_set(const hash_set&) noexcept = default;
        hash_set(hash_set&&) noexcept = default;
        hash_set& operator=(const hash_set&) noexcept = default;
        hash_set& operator=(hash_set&&) noexcept = default;
        ~hash_set() = default;

        [[nodiscard]] size_type size() const noexcept { return m_map.size(); }

        [[nodiscard]] size_type capacity() const noexcept { return m_map.capacity(); }

        [[nodiscard]] bool empty() const noexcept { return m_map.empty(); }

        void clear() noexcept { m_map.clear(); }

        void reserve(size_type new_cap) noexcept { m_map.reserve(new_cap); }

        bool insert(const K& key) noexcept { return m_map.emplace(key).inserted; }

        bool insert(K&& key) noexcept { return m_map.emplace(static_cast<K&&>(key)).inserted; }

        [[nodiscard]] bool contains(const K& key) const noexcept { return m_map.find(key) != nullptr; }

        bool erase(const K& key) noexcept { return m_map.erase(key); }

    private:
        hash_map<K, monostate, Hash> m_map{};
    };

}
