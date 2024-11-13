// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "atomic.h"
#include "memory.h"

#include "core/dh/type_traits.h"

namespace dh {

    // one writer only; payload reads still need to be atomic.
    template <typename T> class seqlock final {
    public:
        static_assert(dh::is_trivially_copyable_v<T>, "dh::seqlock needs a trivially copyable type");

        static_assert(dh::is_nothrow_default_constructible_v<T>);

        constexpr seqlock() noexcept = default;
        ~seqlock() = default;

        seqlock(const seqlock&) = delete;
        seqlock& operator=(const seqlock&) = delete;
        seqlock(seqlock&&) = delete;
        seqlock& operator=(seqlock&&) = delete;

        void store(const T& value) noexcept {
            dh::u64 words[k_word_count]{};
            dh::memcpy(words, &value, sizeof(T));
            const dh::u64 seq = m_seq.load(dh::memory_order_relaxed);
            m_seq.store(seq + 1, dh::memory_order_relaxed);
            dh::atomic_thread_fence(dh::memory_order_release);
            for (dh::size_t i = 0; i < k_word_count; ++i) {
                m_words[i].store(words[i], dh::memory_order_relaxed);
            }
            m_seq.store(seq + 2, dh::memory_order_release);
            m_published.store(true, dh::memory_order_release);
        }

        [[nodiscard]] T load() const noexcept {
            T out{};
            if (!m_published.load(dh::memory_order_acquire)) return out;
            dh::u64 words[k_word_count]{};
            for (;;) {
                const dh::u64 before = m_seq.load(dh::memory_order_acquire);
                if ((before & 1) != 0) continue;
                for (dh::size_t i = 0; i < k_word_count; ++i) {
                    words[i] = m_words[i].load(dh::memory_order_relaxed);
                }
                dh::atomic_thread_fence(dh::memory_order_acquire);
                if (m_seq.load(dh::memory_order_relaxed) == before) {
                    dh::memcpy(&out, words, sizeof(T));
                    return out;
                }
            }
        }

    private:
        static constexpr dh::size_t k_word_count = (sizeof(T) - 1) / sizeof(dh::u64) + 1;
        dh::atomic<dh::u64> m_seq{0};
        dh::atomic<dh::u64> m_words[k_word_count]{};
        dh::atomic<bool> m_published{false};
    };

}
