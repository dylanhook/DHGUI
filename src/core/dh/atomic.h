// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/memory.h"

#include <intrin.h>
#include "core/dh/type_traits.h"

namespace dh {

    enum class memory_order : int {
        relaxed,
        consume,
        acquire,
        release,
        acq_rel,
        seq_cst,
    };

    inline constexpr memory_order memory_order_relaxed = memory_order::relaxed;
    inline constexpr memory_order memory_order_consume = memory_order::consume;
    inline constexpr memory_order memory_order_acquire = memory_order::acquire;
    inline constexpr memory_order memory_order_release = memory_order::release;
    inline constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
    inline constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;

    namespace detail {

        template <dh::size_t N> struct atomic_repr;

        template <> struct atomic_repr<1> {
            using type = char;
        };

        template <> struct atomic_repr<2> {
            using type = short;
        };

        template <> struct atomic_repr<4> {
            using type = long;
        };

        template <> struct atomic_repr<8> {
            using type = __int64;
        };

        template <typename T> using atomic_repr_t = typename atomic_repr<sizeof(T)>::type;

        template <typename T> [[nodiscard]] inline atomic_repr_t<T> to_repr(T v) noexcept {
            atomic_repr_t<T> r{};
            dh::memcpy(&r, &v, sizeof(T));
            return r;
        }

        template <typename T> [[nodiscard]] inline T from_repr(atomic_repr_t<T> r) noexcept {
            T v{};
            dh::memcpy(&v, &r, sizeof(T));
            return v;
        }

        [[nodiscard]] constexpr bool is_acquire(memory_order o) noexcept {
            return o == memory_order::acquire || o == memory_order::consume || o == memory_order::acq_rel || o == memory_order::seq_cst;
        }

        [[nodiscard]] constexpr bool is_release(memory_order o) noexcept {
            return o == memory_order::release || o == memory_order::acq_rel || o == memory_order::seq_cst;
        }

        template <typename R> [[nodiscard]] inline R raw_load(const volatile R* p) noexcept {
            if constexpr (sizeof(R) == 1) {
                return static_cast<R>(__iso_volatile_load8(reinterpret_cast<const volatile __int8*>(p)));
            } else if constexpr (sizeof(R) == 2) {
                return static_cast<R>(__iso_volatile_load16(reinterpret_cast<const volatile __int16*>(p)));
            } else if constexpr (sizeof(R) == 4) {
                return static_cast<R>(__iso_volatile_load32(reinterpret_cast<const volatile __int32*>(p)));
            } else {
                return static_cast<R>(__iso_volatile_load64(reinterpret_cast<const volatile __int64*>(p)));
            }
        }

        template <typename R> inline void raw_store(volatile R* p, R v) noexcept {
            if constexpr (sizeof(R) == 1) {
                __iso_volatile_store8(reinterpret_cast<volatile __int8*>(p), static_cast<__int8>(v));
            } else if constexpr (sizeof(R) == 2) {
                __iso_volatile_store16(reinterpret_cast<volatile __int16*>(p), static_cast<__int16>(v));
            } else if constexpr (sizeof(R) == 4) {
                __iso_volatile_store32(reinterpret_cast<volatile __int32*>(p), static_cast<__int32>(v));
            } else {
                __iso_volatile_store64(reinterpret_cast<volatile __int64*>(p), static_cast<__int64>(v));
            }
        }

        template <typename R> [[nodiscard]] inline R raw_exchange(volatile R* p, R v) noexcept {
            if constexpr (sizeof(R) == 1) {
                return static_cast<R>(_InterlockedExchange8(reinterpret_cast<volatile char*>(p), static_cast<char>(v)));
            } else if constexpr (sizeof(R) == 2) {
                return static_cast<R>(_InterlockedExchange16(reinterpret_cast<volatile short*>(p), static_cast<short>(v)));
            } else if constexpr (sizeof(R) == 4) {
                return static_cast<R>(_InterlockedExchange(reinterpret_cast<volatile long*>(p), static_cast<long>(v)));
            } else {
                return static_cast<R>(_InterlockedExchange64(reinterpret_cast<volatile __int64*>(p), static_cast<__int64>(v)));
            }
        }

        template <typename R> [[nodiscard]] inline R raw_cas(volatile R* p, R desired, R expected) noexcept {
            if constexpr (sizeof(R) == 1) {
                return static_cast<R>(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(p), static_cast<char>(desired), static_cast<char>(expected)));
            } else if constexpr (sizeof(R) == 2) {
                return static_cast<R>(_InterlockedCompareExchange16(reinterpret_cast<volatile short*>(p), static_cast<short>(desired), static_cast<short>(expected)));
            } else if constexpr (sizeof(R) == 4) {
                return static_cast<R>(_InterlockedCompareExchange(reinterpret_cast<volatile long*>(p), static_cast<long>(desired), static_cast<long>(expected)));
            } else {
                return static_cast<R>(_InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(p), static_cast<__int64>(desired), static_cast<__int64>(expected)));
            }
        }

        template <typename R> [[nodiscard]] inline R raw_fetch_add(volatile R* p, R v) noexcept {
            if constexpr (sizeof(R) == 1) {
                return static_cast<R>(_InterlockedExchangeAdd8(reinterpret_cast<volatile char*>(p), static_cast<char>(v)));
            } else if constexpr (sizeof(R) == 2) {
                return static_cast<R>(_InterlockedExchangeAdd16(reinterpret_cast<volatile short*>(p), static_cast<short>(v)));
            } else if constexpr (sizeof(R) == 4) {
                return static_cast<R>(_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(p), static_cast<long>(v)));
            } else {
                return static_cast<R>(_InterlockedExchangeAdd64(reinterpret_cast<volatile __int64*>(p), static_cast<__int64>(v)));
            }
        }

    }

    inline void atomic_thread_fence(memory_order order) noexcept {
        if (order == memory_order::relaxed) return;
        _ReadWriteBarrier();
        if (order == memory_order::seq_cst) {
            _mm_mfence();
        }
    }

    template <typename T> class atomic final {
    public:
        using value_type = T;

        static_assert(dh::is_trivially_copyable_v<T>, "dh::atomic needs a trivially copyable type");
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "dh::atomic only supports sizes 1 / 2 / 4 / 8");
        static_assert(alignof(T) >= sizeof(T), "dh::atomic needs natural alignment");

        constexpr atomic() noexcept : m_value{} {}

        constexpr atomic(T desired) noexcept : m_value(desired) {}

        atomic(const atomic&) = delete;
        atomic& operator=(const atomic&) = delete;
        atomic(atomic&&) = delete;
        atomic& operator=(atomic&&) = delete;
        ~atomic() = default;

        T operator=(T desired) noexcept {
            store(desired);
            return desired;
        }

        operator T() const noexcept { return load(); }

        [[nodiscard]] T load(memory_order order = memory_order::seq_cst) const noexcept {
            const auto r = detail::raw_load(repr_ptr());
            if (detail::is_acquire(order)) {
                _ReadWriteBarrier();
            }
            return detail::from_repr<T>(r);
        }

        void store(T desired, memory_order order = memory_order::seq_cst) noexcept {
            const auto r = detail::to_repr(desired);
            if (order == memory_order::seq_cst) {
                (void)detail::raw_exchange(repr_ptr(), r);
                return;
            }
            if (detail::is_release(order)) {
                _ReadWriteBarrier();
            }
            detail::raw_store(repr_ptr(), r);
        }

        T exchange(T desired, memory_order = memory_order::seq_cst) noexcept {
            return detail::from_repr<T>(detail::raw_exchange(repr_ptr(), detail::to_repr(desired)));
        }

        [[nodiscard]] bool compare_exchange_strong(T& expected, T desired, memory_order = memory_order::seq_cst) noexcept {
            const auto want = detail::to_repr(expected);
            const auto prev = detail::raw_cas(repr_ptr(), detail::to_repr(desired), want);
            if (prev == want) {
                return true;
            }
            expected = detail::from_repr<T>(prev);
            return false;
        }

        [[nodiscard]] bool compare_exchange_strong(T& expected, T desired, memory_order  , memory_order) noexcept {
            return compare_exchange_strong(expected, desired, memory_order::seq_cst);
        }

        [[nodiscard]] bool compare_exchange_weak(T& expected, T desired, memory_order order = memory_order::seq_cst) noexcept {
            return compare_exchange_strong(expected, desired, order);
        }

        [[nodiscard]] bool compare_exchange_weak(T& expected, T desired, memory_order success, memory_order failure) noexcept {
            return compare_exchange_strong(expected, desired, success, failure);
        }

        T fetch_add(T arg, memory_order = memory_order::seq_cst) noexcept {
            static_assert(dh::is_integral_v<T>, "fetch_add needs an integer type");
            return detail::from_repr<T>(detail::raw_fetch_add(repr_ptr(), detail::to_repr(arg)));
        }

        T fetch_sub(T arg, memory_order = memory_order::seq_cst) noexcept {
            static_assert(dh::is_integral_v<T>, "fetch_sub needs an integer type");

            using R = detail::atomic_repr_t<T>;
            using U = dh::make_unsigned_t<R>;
            const U bits = static_cast<U>(U{0} - static_cast<U>(detail::to_repr(arg)));
            const R neg = detail::to_repr(bits);
            return detail::from_repr<T>(detail::raw_fetch_add(repr_ptr(), neg));
        }

    private:
        using repr_t = detail::atomic_repr_t<T>;

        [[nodiscard]] volatile repr_t* repr_ptr() const noexcept {
            return reinterpret_cast<volatile repr_t*>(const_cast<T*>(&m_value));
        }

        alignas(sizeof(T)) T m_value;
    };

    template <typename T> class atomic_ref final {
    public:
        using value_type = T;

        static_assert(dh::is_trivially_copyable_v<T>, "dh::atomic_ref needs a trivially copyable type");
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "dh::atomic_ref only supports sizes 1 / 2 / 4 / 8");

        static constexpr dh::size_t required_alignment = sizeof(T);

        explicit atomic_ref(T& obj) noexcept : m_ptr(&obj) {}

        atomic_ref(const atomic_ref&) = default;
        atomic_ref& operator=(const atomic_ref&) = delete;
        atomic_ref(atomic_ref&&) = default;
        atomic_ref& operator=(atomic_ref&&) = delete;
        ~atomic_ref() = default;

        [[nodiscard]] T load(memory_order order = memory_order::seq_cst) const noexcept {
            const auto r = detail::raw_load(repr_ptr());
            if (detail::is_acquire(order)) {
                _ReadWriteBarrier();
            }
            return detail::from_repr<T>(r);
        }

        void store(T desired, memory_order order = memory_order::seq_cst) const noexcept {
            const auto r = detail::to_repr(desired);
            if (order == memory_order::seq_cst) {
                (void)detail::raw_exchange(repr_ptr(), r);
                return;
            }
            if (detail::is_release(order)) {
                _ReadWriteBarrier();
            }
            detail::raw_store(repr_ptr(), r);
        }

    private:
        using repr_t = detail::atomic_repr_t<T>;

        [[nodiscard]] volatile repr_t* repr_ptr() const noexcept { return reinterpret_cast<volatile repr_t*>(m_ptr); }

        T* m_ptr;
    };

}
