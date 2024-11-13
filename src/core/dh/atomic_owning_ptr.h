// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/expected.h"
#include "core/dh/string.h"
#include "core/dh/unique_ptr.h"

#include "core/dh/atomic.h"

namespace dh {

    enum class owning_ptr_error : unsigned char {
        already_published,
        null_input,
    };

    [[nodiscard]] constexpr dh::string_view to_string(owning_ptr_error e) noexcept {
        switch (e) {
            case owning_ptr_error::already_published:
                return "already_published";
            case owning_ptr_error::null_input:
                return "null_input";
        }
        return "unknown";
    }

    template <typename T> class atomic_owning_ptr final {
    public:
        using element_type = T;

        constexpr atomic_owning_ptr() noexcept : m_slot(nullptr), m_alive(false) {}

        atomic_owning_ptr(atomic_owning_ptr&& other) noexcept : m_slot(other.m_slot.exchange(nullptr, dh::memory_order_acq_rel)), m_alive(other.m_alive.exchange(false, dh::memory_order_acq_rel)) {}

        atomic_owning_ptr& operator=(atomic_owning_ptr&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            T* incoming = other.m_slot.exchange(nullptr, dh::memory_order_acq_rel);
            bool incoming_alive = other.m_alive.exchange(false, dh::memory_order_acq_rel);
            m_alive.store(false, dh::memory_order_release);
            T* leftover = m_slot.exchange(incoming, dh::memory_order_acq_rel);
            if (leftover != nullptr) {
                dh::default_delete<T>{}(leftover);
            }
            m_alive.store(incoming_alive, dh::memory_order_release);
            return *this;
        }

        atomic_owning_ptr(const atomic_owning_ptr&) = delete;
        atomic_owning_ptr& operator=(const atomic_owning_ptr&) = delete;

        ~atomic_owning_ptr() noexcept {
            m_alive.store(false, dh::memory_order_release);
            T* leftover = m_slot.exchange(nullptr, dh::memory_order_acq_rel);
            if (leftover != nullptr) {
                dh::default_delete<T>{}(leftover);
            }
        }

        [[nodiscard]] dh::expected<void, owning_ptr_error> publish(dh::unique_ptr<T>&& owner) noexcept {
            if (!owner) {
                return dh::unexpected<owning_ptr_error>{owning_ptr_error::null_input};
            }
            T* raw = owner.get();

            T* cas_expected = nullptr;
            bool ok = m_slot.compare_exchange_strong(cas_expected, raw, dh::memory_order_release, dh::memory_order_acquire);
            if (!ok) {
                return dh::unexpected<owning_ptr_error>{owning_ptr_error::already_published};
            }

            m_alive.store(true, dh::memory_order_release);
            (void)owner.release();
            return {};
        }

        [[nodiscard]] T* observe() const noexcept {
            if (!m_alive.load(dh::memory_order_acquire)) {
                return nullptr;
            }
            return m_slot.load(dh::memory_order_acquire);
        }

        // stop readers before drain(), this pointer doesnt wait for them.
        [[nodiscard]] dh::unique_ptr<T> drain() noexcept {
            m_alive.exchange(false, dh::memory_order_acq_rel);
            T* raw = m_slot.exchange(nullptr, dh::memory_order_acq_rel);
            return dh::unique_ptr<T>{raw};
        }

        [[nodiscard]] bool empty() const noexcept { return m_slot.load(dh::memory_order_acquire) == nullptr; }

    private:
        dh::atomic<T*> m_slot;
        dh::atomic<bool> m_alive;
    };

}
