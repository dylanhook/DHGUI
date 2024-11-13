// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "runtime.h"

#include "core/dh/atomic.h"

#include "core/dh/concepts.h"
#include "core/dh/type_traits.h"
#include "core/dh/utility.h"
#include "core/dh/string.h"
#include "math/color.h"
#include <Windows.h>

namespace dh::reactive {

    namespace detail {

        template <typename T> inline constexpr bool signal_is_atomic_v =
            dh::is_trivially_copyable_v<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) && alignof(T) >= sizeof(T);

        template <typename T> inline constexpr bool signal_is_overrideable_v =
            dh::is_same_v<T, bool> || dh::is_same_v<T, float> || dh::is_same_v<T, int> || dh::is_same_v<T, dh::u64> ||
            dh::is_same_v<T, dh::string> || dh::is_same_v<T, math::color>;

        template <typename T, bool Enabled = signal_is_overrideable_v<T>,
            bool Wide = sizeof(T) == sizeof(dh::u64), bool Atomic = signal_is_atomic_v<T>> class signal_override final {
        public:
            explicit signal_override(T) noexcept {}

            ~signal_override() = default;

            signal_override(const signal_override&) = delete;
            signal_override& operator=(const signal_override&) = delete;
            signal_override(signal_override&&) = delete;
            signal_override& operator=(signal_override&&) = delete;

            [[nodiscard]] bool active() const noexcept { return false; }

            [[nodiscard]] T value() const noexcept { return T{}; }
        };

        template <typename T> class signal_override<T, true, false, true> final {
        public:
            explicit signal_override(T initial) noexcept : m_packed(pack(initial, false)) {}

            ~signal_override() = default;

            signal_override(const signal_override&) = delete;
            signal_override& operator=(const signal_override&) = delete;
            signal_override(signal_override&&) = delete;
            signal_override& operator=(signal_override&&) = delete;

            [[nodiscard]] bool active() const noexcept { return (m_packed.load(dh::memory_order_acquire) & k_active_bit) != 0; }

            [[nodiscard]] T value() const noexcept { return unpack(m_packed.load(dh::memory_order_acquire)); }

            [[nodiscard]] T effective(T base) const noexcept {
                const dh::u64 packed = m_packed.load(dh::memory_order_acquire);
                return (packed & k_active_bit) != 0 ? unpack(packed) : base;
            }

            void set(T value) noexcept { m_packed.store(pack(value, true), dh::memory_order_release); }

            void clear(T base) noexcept { m_packed.store(pack(base, false), dh::memory_order_release); }

        private:
            static constexpr dh::u64 k_active_bit = 1ull << 63;

            [[nodiscard]] static dh::u64 pack(T value, bool active) noexcept {
                dh::u64 packed = 0;
                dh::memcpy(&packed, &value, sizeof(T));
                return active ? packed | k_active_bit : packed;
            }

            [[nodiscard]] static T unpack(dh::u64 packed) noexcept {
                T value{};
                dh::memcpy(&value, &packed, sizeof(T));
                return value;
            }

            dh::atomic<dh::u64> m_packed;
        };

        template <typename T> class signal_override<T, true, true, true> final {
        public:
            explicit signal_override(T initial) noexcept : m_value(initial) {}

            ~signal_override() = default;

            signal_override(const signal_override&) = delete;
            signal_override& operator=(const signal_override&) = delete;
            signal_override(signal_override&&) = delete;
            signal_override& operator=(signal_override&&) = delete;

            [[nodiscard]] bool active() const noexcept { return read().active; }

            [[nodiscard]] T value() const noexcept { return read().value; }

            [[nodiscard]] T effective(T base) const noexcept {
                const override_snapshot snapshot = read();
                return snapshot.active ? snapshot.value : base;
            }

            void set(T value) noexcept { publish(value, true); }

            void clear(T base) noexcept { publish(base, false); }

        private:
            struct override_snapshot final {
            public:
                override_snapshot() noexcept = default;
                ~override_snapshot() = default;
                override_snapshot(const override_snapshot&) noexcept = default;
                override_snapshot& operator=(const override_snapshot&) noexcept = default;
                override_snapshot(override_snapshot&&) noexcept = default;
                override_snapshot& operator=(override_snapshot&&) noexcept = default;

                T value{};
                bool active{false};
            };

            [[nodiscard]] override_snapshot read() const noexcept {
                for (;;) {
                    const dh::u64 before = m_sequence.load(dh::memory_order_acquire);
                    if ((before & 1u) != 0) continue;

                    override_snapshot snapshot;
                    snapshot.value = m_value.load(dh::memory_order_relaxed);
                    snapshot.active = m_active.load(dh::memory_order_relaxed);

                    const dh::u64 after = m_sequence.load(dh::memory_order_acquire);
                    if (before == after) return snapshot;
                }
            }

            void publish(T value, bool active) noexcept {
                const dh::u64 writing = m_sequence.fetch_add(1, dh::memory_order_acq_rel) + 1;
                m_value.store(value, dh::memory_order_relaxed);
                m_active.store(active, dh::memory_order_relaxed);
                m_sequence.store(writing + 1, dh::memory_order_release);
            }

            dh::atomic<dh::u64> m_sequence{0};
            dh::atomic<T> m_value;
            dh::atomic<bool> m_active{false};
        };

        template <typename T, bool Wide> class signal_override<T, true, Wide, false> final {
        public:
            explicit signal_override(const T&) noexcept {}
            ~signal_override() = default;
            signal_override(const signal_override&) = delete;
            signal_override& operator=(const signal_override&) = delete;
            signal_override(signal_override&&) = delete;
            signal_override& operator=(signal_override&&) = delete;

            [[nodiscard]] bool active() const noexcept { return m_active; }
            [[nodiscard]] const T& value() const noexcept { return m_value; }
            [[nodiscard]] const T& effective(const T& base) const noexcept { return m_active ? m_value : base; }
            void set(const T& value) noexcept { m_value = T{value}; m_active = true; }
            void clear(const T&) noexcept { m_active = false; m_value = T{}; }

        private:
            T m_value{};
            bool m_active{false};
        };

    }

    // atomic storage covers the value only, writes and subscriptions still belong on the ui thread.
    template <typename T> class signal final : public detail::signal_base {
    public:
        static constexpr bool k_atomic = detail::signal_is_atomic_v<T>;
        using storage_type = dh::conditional_t<k_atomic, dh::atomic<T>, T>;
        using peek_type = dh::conditional_t<k_atomic, T, const T&>;

        explicit signal(T initial = T{}) noexcept : m_value(dh::move(initial)), m_override(initial) {}

        ~signal() override = default;

        signal(const signal&) = delete;
        signal& operator=(const signal&) = delete;
        signal(signal&&) = delete;
        signal& operator=(signal&&) = delete;

        [[nodiscard]] T operator()() const noexcept {
            track_observation(detail::signal_views::effective);
            return load();
        }

        [[nodiscard]] T base() const noexcept {
            track_observation(detail::signal_views::base);
            return load_base();
        }

        // peek skips dependency tracking, borrowed strings only last until the next write.
        [[nodiscard]] peek_type peek() const noexcept { return load(); }

        [[nodiscard]] peek_type peek_base() const noexcept { return load_base(); }

        [[nodiscard]] dh::u64 write_revision() const noexcept { return m_write_revision; }

        [[nodiscard]] bool override_active() const noexcept
            requires detail::signal_is_overrideable_v<T>
        {
            return m_override.active();
        }

        // overrides change what readers see without replacing the saved value.
        void set_override(const T& value) noexcept
            requires detail::signal_is_overrideable_v<T> && dh::equality_comparable<T>
        {
            const bool changed = load() != value;
            if (m_override.active() && !changed) return;
            m_override.set(value);
            if (changed) notify_subscribers(detail::signal_views::effective);
        }

        void clear_override() noexcept
            requires detail::signal_is_overrideable_v<T> && dh::equality_comparable<T>
        {
            if (!m_override.active()) return;
            const bool changed = m_override.value() != load_base();
            m_override.clear(load_base());
            if (changed) notify_subscribers(detail::signal_views::effective);
        }

        void set(T value) noexcept
            requires dh::equality_comparable<T>
        {
            if (m_write_revision == ~dh::u64{0}) dh::fail_fast("signal write counter overflow", FAST_FAIL_INVALID_ARG);
            ++m_write_revision;
            if (load_base() == value) return;
            if constexpr (k_atomic) {
                m_value.store(value, dh::memory_order_release);
            } else {
                m_value = dh::move(value);
            }
            // base readers still need updates when an override hides the new value.
            notify_subscribers(m_override.active() ? detail::signal_views::base : detail::signal_views::both);
        }

    private:
        [[nodiscard]] peek_type load() const noexcept {
            if constexpr (detail::signal_is_overrideable_v<T>) {
                return m_override.effective(load_base());
            } else {
                return load_base();
            }
        }

        [[nodiscard]] peek_type load_base() const noexcept {
            if constexpr (k_atomic) {
                return m_value.load(dh::memory_order_acquire);
            } else {
                return m_value;
            }
        }

        storage_type m_value;
        dh::u64 m_write_revision{1};

        [[msvc::no_unique_address]] detail::signal_override<T> m_override;
    };

}
