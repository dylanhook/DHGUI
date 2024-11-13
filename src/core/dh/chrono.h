// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "expected.h"
#include "memory.h"
#include "numeric_limits.h"
#include "string.h"

#include "core/dh/atomic.h"
#include "core/dh/ratio.h"

namespace dh::chrono {

    namespace detail {

        template <typename Dest, typename Factor, typename Src> [[nodiscard]] constexpr Dest scale_ticks(Src ticks) noexcept {
            static_assert(dh::is_integral_v<Src> && dh::is_integral_v<Dest>);
            static_assert(sizeof(Src) <= sizeof(dh::u64) && sizeof(Dest) <= sizeof(dh::u64));
            static_assert(Factor::num > 0 && Factor::den > 0);
            const bool negative = dh::is_signed_v<Src> && ticks < 0;
            const dh::u64 magnitude = negative ? dh::u64{0} - static_cast<dh::u64>(ticks) : static_cast<dh::u64>(ticks);
            constexpr dh::u64 num = Factor::num;
            constexpr dh::u64 den = Factor::den;
            dh::u64 scaled = 0;
            if (magnitude <= dh::numeric_limits<dh::u64>::max() / num) {
                scaled = magnitude * num / den;
            } else {
                dh::u64 high = 0;
                dh::u64 low = 0;
                if consteval {
                    constexpr dh::u64 mask = 0xffffffffULL;
                    const dh::u64 bottom = (magnitude & mask) * (num & mask);
                    const dh::u64 cross_a = (magnitude >> 32) * (num & mask);
                    const dh::u64 cross_b = (magnitude & mask) * (num >> 32);
                    const dh::u64 middle = (bottom >> 32) + (cross_a & mask) + (cross_b & mask);
                    low = (middle << 32) | (bottom & mask);
                    high = (magnitude >> 32) * (num >> 32) + (cross_a >> 32) + (cross_b >> 32) + (middle >> 32);
                } else {
                    low = _umul128(magnitude, num, &high);
                }
                if (high >= den) {
                    return negative ? dh::numeric_limits<Dest>::lowest() : dh::numeric_limits<Dest>::max();
                }
                if consteval {
                    for (int bit = 63; bit >= 0; --bit) {
                        high = (high << 1) | ((low >> bit) & 1);
                        if (high >= den) {
                            high -= den;
                            scaled |= dh::u64{1} << bit;
                        }
                    }
                } else {
                    dh::u64 remainder = 0;
                    scaled = _udiv128(high, low, den, &remainder);
                }
            }
            if (negative) {
                if constexpr (!dh::is_signed_v<Dest>)
                    return Dest{0};
                else {
                    constexpr dh::u64 limit = dh::u64{0} - static_cast<dh::u64>(dh::numeric_limits<Dest>::lowest());
                    if (scaled >= limit) return dh::numeric_limits<Dest>::lowest();
                    return static_cast<Dest>(-static_cast<dh::i64>(scaled));
                }
            }
            if (scaled > static_cast<dh::u64>(dh::numeric_limits<Dest>::max())) {
                return dh::numeric_limits<Dest>::max();
            }
            return static_cast<Dest>(scaled);
        }

    }

    template <typename Rep, typename Period = dh::ratio<1>> class duration final {
    public:
        static_assert(dh::is_integral_v<Rep> && sizeof(Rep) <= sizeof(dh::u64));
        static_assert(Period::num > 0 && Period::den > 0);
        using rep = Rep;
        using period = Period;

        constexpr duration() noexcept = default;
        constexpr duration(const duration&) noexcept = default;
        constexpr duration(duration&&) noexcept = default;
        constexpr duration& operator=(const duration&) noexcept = default;
        constexpr duration& operator=(duration&&) noexcept = default;
        ~duration() = default;

        constexpr explicit duration(rep ticks) noexcept : m_ticks(ticks) {}

        template <typename Rep2, typename Period2>
            requires(dh::ratio_divide<Period2, Period>::den == 1)
        constexpr duration(const duration<Rep2, Period2>& other) noexcept : m_ticks(scale_from<Rep2, Period2>(other.count())) {}

        [[nodiscard]] constexpr rep count() const noexcept { return m_ticks; }

        constexpr duration& operator+=(duration rhs) noexcept {
            m_ticks += rhs.m_ticks;
            return *this;
        }

        constexpr duration& operator-=(duration rhs) noexcept {
            m_ticks -= rhs.m_ticks;
            return *this;
        }

        [[nodiscard]] friend constexpr duration operator+(duration a, duration b) noexcept { return duration{static_cast<rep>(a.m_ticks + b.m_ticks)}; }

        [[nodiscard]] friend constexpr duration operator-(duration a, duration b) noexcept { return duration{static_cast<rep>(a.m_ticks - b.m_ticks)}; }

        [[nodiscard]] friend constexpr bool operator==(duration a, duration b) noexcept { return a.m_ticks == b.m_ticks; }

        [[nodiscard]] friend constexpr bool operator!=(duration a, duration b) noexcept { return a.m_ticks != b.m_ticks; }

        [[nodiscard]] friend constexpr bool operator<(duration a, duration b) noexcept { return a.m_ticks < b.m_ticks; }

        [[nodiscard]] friend constexpr bool operator<=(duration a, duration b) noexcept { return a.m_ticks <= b.m_ticks; }

        [[nodiscard]] friend constexpr bool operator>(duration a, duration b) noexcept { return a.m_ticks > b.m_ticks; }

        [[nodiscard]] friend constexpr bool operator>=(duration a, duration b) noexcept { return a.m_ticks >= b.m_ticks; }

    private:
        template <typename Rep2, typename Period2> [[nodiscard]] static constexpr rep scale_from(Rep2 src) noexcept {
            using factor = dh::ratio_divide<Period2, Period>;
            return detail::scale_ticks<rep, factor>(src);
        }

        rep m_ticks{0};
    };

    using nanoseconds = duration<dh::i64, dh::nano>;
    using milliseconds = duration<dh::i64, dh::milli>;
    using seconds = duration<dh::i64, dh::ratio<1>>;

    template <typename DestDur, typename Rep, typename Period> [[nodiscard]] constexpr DestDur duration_cast(const duration<Rep, Period>& src) noexcept {
        using factor = dh::ratio_divide<Period, typename DestDur::period>;
        return DestDur{detail::scale_ticks<typename DestDur::rep, factor>(src.count())};
    }

    template <typename Clock, typename Duration = typename Clock::duration> class time_point final {
    public:
        using clock = Clock;
        using duration = Duration;
        using rep = typename Duration::rep;
        using period = typename Duration::period;

        constexpr time_point() noexcept = default;
        constexpr time_point(const time_point&) noexcept = default;
        constexpr time_point(time_point&&) noexcept = default;
        constexpr time_point& operator=(const time_point&) noexcept = default;
        constexpr time_point& operator=(time_point&&) noexcept = default;
        ~time_point() = default;

        constexpr explicit time_point(Duration d) noexcept : m_since_epoch(d) {}

        template <typename Duration2> constexpr time_point(const time_point<Clock, Duration2>& other) noexcept : m_since_epoch(Duration{other.time_since_epoch()}) {}

        [[nodiscard]] constexpr Duration time_since_epoch() const noexcept { return m_since_epoch; }

        constexpr time_point& operator+=(Duration d) noexcept {
            m_since_epoch += d;
            return *this;
        }

        constexpr time_point& operator-=(Duration d) noexcept {
            m_since_epoch -= d;
            return *this;
        }

        [[nodiscard]] friend constexpr time_point operator+(time_point t, Duration d) noexcept { return time_point{t.m_since_epoch + d}; }

        [[nodiscard]] friend constexpr time_point operator+(Duration d, time_point t) noexcept { return time_point{t.m_since_epoch + d}; }

        [[nodiscard]] friend constexpr time_point operator-(time_point t, Duration d) noexcept { return time_point{t.m_since_epoch - d}; }

        [[nodiscard]] friend constexpr Duration operator-(time_point a, time_point b) noexcept { return a.m_since_epoch - b.m_since_epoch; }

        [[nodiscard]] friend constexpr bool operator==(time_point a, time_point b) noexcept { return a.m_since_epoch == b.m_since_epoch; }

        [[nodiscard]] friend constexpr bool operator!=(time_point a, time_point b) noexcept { return a.m_since_epoch != b.m_since_epoch; }

        [[nodiscard]] friend constexpr bool operator<(time_point a, time_point b) noexcept { return a.m_since_epoch < b.m_since_epoch; }

        [[nodiscard]] friend constexpr bool operator<=(time_point a, time_point b) noexcept { return a.m_since_epoch <= b.m_since_epoch; }

        [[nodiscard]] friend constexpr bool operator>(time_point a, time_point b) noexcept { return a.m_since_epoch > b.m_since_epoch; }

        [[nodiscard]] friend constexpr bool operator>=(time_point a, time_point b) noexcept { return a.m_since_epoch >= b.m_since_epoch; }

    private:
        Duration m_since_epoch{};
    };

    enum class init_error : dh::u8 {
        qpc_unavailable,
    };

    [[nodiscard]] constexpr dh::string_view to_string(init_error e) noexcept {
        switch (e) {
            case init_error::qpc_unavailable:
                return "qpc_unavailable";
        }
        return "unknown";
    }

    namespace detail {

        constinit inline dh::atomic<dh::i64> g_qpc_freq_hz{0};

        [[nodiscard]] inline dh::i64 qpc_freq_hz() noexcept {
            const dh::i64 freq = g_qpc_freq_hz.load(dh::memory_order_acquire);
            if (freq == 0) {
                dh::fail_fast("call prime_qpc_frequency before reading the clock", FAST_FAIL_INVALID_ARG);
            }
            return freq;
        }

    }

    // prime this before the first steady clock read.
    [[nodiscard]] inline dh::expected<void, init_error> prime_qpc_frequency() noexcept {
        LARGE_INTEGER freq{};
        if (::QueryPerformanceFrequency(&freq) == FALSE || freq.QuadPart == 0) {
            return dh::unexpected<init_error>{init_error::qpc_unavailable};
        }
        detail::g_qpc_freq_hz.store(freq.QuadPart, dh::memory_order_release);
        return {};
    }

    namespace detail {

        inline constexpr dh::i64 k_filetime_unix_epoch_offset = 116444736000000000ll;

        inline constexpr dh::i64 k_ns_per_second = dh::nano::den;

        inline constexpr dh::i64 k_filetime_tick_ns = 100;

    }

    class steady_clock final {
    public:
        using rep = dh::i64;
        using period = dh::nano;
        using duration = nanoseconds;
        using time_point = dh::chrono::time_point<steady_clock, nanoseconds>;

        static constexpr bool is_steady = true;

        steady_clock() = delete;
        steady_clock(const steady_clock&) = delete;
        steady_clock(steady_clock&&) = delete;
        steady_clock& operator=(const steady_clock&) = delete;
        steady_clock& operator=(steady_clock&&) = delete;
        ~steady_clock() = delete;

        [[nodiscard]] static time_point now() noexcept {
            const dh::i64 freq = detail::qpc_freq_hz();

            LARGE_INTEGER counter{};
            ::QueryPerformanceCounter(&counter);

            dh::u64 product_hi = 0;
            const dh::u64 product_lo = _umul128(static_cast<dh::u64>(counter.QuadPart), static_cast<dh::u64>(detail::k_ns_per_second), &product_hi);

            dh::u64 remainder = 0;
            const dh::u64 ns = _udiv128(product_hi, product_lo, static_cast<dh::u64>(freq), &remainder);

            return time_point{nanoseconds{static_cast<dh::i64>(ns)}};
        }
    };

    class system_clock final {
    public:
        using rep = dh::i64;
        using period = dh::nano;
        using duration = nanoseconds;
        using time_point = dh::chrono::time_point<system_clock, nanoseconds>;

        static constexpr bool is_steady = false;

        system_clock() = delete;
        system_clock(const system_clock&) = delete;
        system_clock(system_clock&&) = delete;
        system_clock& operator=(const system_clock&) = delete;
        system_clock& operator=(system_clock&&) = delete;
        ~system_clock() = delete;

        [[nodiscard]] static time_point now() noexcept {
            FILETIME ft{};
            ::GetSystemTimePreciseAsFileTime(&ft);

            dh::u64 ticks_u = 0;
            dh::memcpy(&ticks_u, &ft, sizeof(ticks_u));
            const dh::i64 ns_since_unix_epoch = ticks_u >= detail::k_filetime_unix_epoch_offset ?
                detail::scale_ticks<dh::i64, dh::ratio<detail::k_filetime_tick_ns>>(ticks_u - detail::k_filetime_unix_epoch_offset) : detail::scale_ticks<dh::i64, dh::ratio<detail::k_filetime_tick_ns>>(-static_cast<dh::i64>(detail::k_filetime_unix_epoch_offset - ticks_u));
            return time_point{nanoseconds{ns_since_unix_epoch}};
        }

        [[nodiscard]] static dh::i64 to_unix_seconds(time_point tp) noexcept {
            const dh::i64 ns = tp.time_since_epoch().count();
            return ns / detail::k_ns_per_second - (ns < 0 && ns % detail::k_ns_per_second != 0 ? 1 : 0);
        }

        [[nodiscard]] static FILETIME to_file_time(time_point tp) noexcept {
            const dh::i64 nanoseconds = tp.time_since_epoch().count();
            const dh::i64 ticks = nanoseconds / detail::k_filetime_tick_ns -
                (nanoseconds < 0 && nanoseconds % detail::k_filetime_tick_ns != 0 ? 1 : 0);
            const dh::u64 absolute = static_cast<dh::u64>(ticks + detail::k_filetime_unix_epoch_offset);
            FILETIME result{};
            dh::memcpy(&result, &absolute, sizeof(result));
            return result;
        }
    };

}
