// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"
#include "type_traits.h"
#include "utility.h"

namespace dh {

    struct less final {
        template <typename A, typename B> [[nodiscard]] constexpr bool operator()(const A& a, const B& b) const noexcept(noexcept(a < b)) { return a < b; }
    };

    struct greater final {
        template <typename A, typename B> [[nodiscard]] constexpr bool operator()(const A& a, const B& b) const noexcept(noexcept(b < a)) { return b < a; }
    };

    template <typename T> [[nodiscard]] constexpr const T& min(const T& a, const T& b) noexcept(noexcept(b < a)) {
        return (b < a) ? b : a;
    }

    template <typename T> [[nodiscard]] constexpr const T& max(const T& a, const T& b) noexcept(noexcept(a < b)) {
        return (a < b) ? b : a;
    }

    template <typename T> [[nodiscard]] constexpr const T& clamp(const T& v, const T& lo, const T& hi) noexcept(noexcept(v < lo)) {
        return (v < lo) ? lo : ((hi < v) ? hi : v);
    }

    template <typename T> [[nodiscard]] constexpr T abs(T v) noexcept {
        static_assert(dh::is_floating_point_v<T>, "dh::abs only supports float types");
        return v < T{0} ? -v : v;
    }

    namespace detail {

        [[nodiscard]] constexpr int log2_floor(dh::size_t n) noexcept {
            int r = 0;
            while (n > 1) {
                n >>= 1;
                ++r;
            }
            return r;
        }

        template <typename Iter> constexpr void iter_swap(Iter a, Iter b) noexcept(noexcept(dh::swap(*a, *b))) {
            if (a != b) dh::swap(*a, *b);
        }

        template <typename Iter, typename Compare> constexpr void median_of_three(Iter a, Iter b, Iter c, Compare& cmp) noexcept(noexcept(cmp(*a, *b))) {
            if (cmp(*b, *a)) {
                dh::detail::iter_swap(a, b);
            }
            if (cmp(*c, *a)) {
                dh::detail::iter_swap(a, c);
            }
            if (cmp(*c, *b)) {
                dh::detail::iter_swap(b, c);
            }
        }

        template <typename Iter, typename Compare>
        [[nodiscard]] constexpr Iter partition(Iter first, Iter last, Compare& cmp) noexcept(noexcept(cmp(*first, *last))) {
            Iter mid = first + (last - first) / 2;
            dh::detail::median_of_three(first, mid, last - 1, cmp);
            dh::detail::iter_swap(first, mid);

            const auto& pivot = *first;
            Iter i = first + 1;
            Iter j = last - 1;
            while (true) {
                while (cmp(*i, pivot)) {
                    ++i;
                }
                while (cmp(pivot, *j)) {
                    --j;
                }
                if (!(i < j)) {
                    return i;
                }
                dh::detail::iter_swap(i, j);
                ++i;
                --j;
            }
        }

        template <typename Iter, typename Compare> constexpr void insertion_sort(Iter first, Iter last, Compare& cmp) noexcept(noexcept(cmp(*first, *first))) {
            for (Iter i = first + 1; i < last; ++i) {
                auto key = dh::move(*i);
                Iter j = i;
                while (j > first && cmp(key, *(j - 1))) {
                    *j = dh::move(*(j - 1));
                    --j;
                }
                *j = static_cast<decltype(key)&&>(key);
            }
        }

        template <typename Iter, typename Compare>
        constexpr void sift_down(Iter first, dh::size_t start, dh::size_t len, Compare& cmp) noexcept(noexcept(cmp(*first, *first))) {
            dh::size_t root = start;
            while (true) {
                dh::size_t child = 2 * root + 1;
                if (child >= len) {
                    return;
                }
                if (child + 1 < len && cmp(*(first + child), *(first + child + 1))) {
                    ++child;
                }
                if (!cmp(*(first + root), *(first + child))) {
                    return;
                }
                dh::detail::iter_swap(first + root, first + child);
                root = child;
            }
        }

        template <typename Iter, typename Compare> constexpr void heap_sort(Iter first, Iter last, Compare& cmp) noexcept(noexcept(cmp(*first, *first))) {
            const dh::size_t len = static_cast<dh::size_t>(last - first);
            if (len < 2) {
                return;
            }
            for (dh::size_t i = len / 2; i-- > 0;) {
                dh::detail::sift_down(first, i, len, cmp);
            }
            for (dh::size_t i = len; i-- > 1;) {
                dh::detail::iter_swap(first, first + i);
                dh::detail::sift_down(first, 0, i, cmp);
            }
        }

        template <typename Iter, typename Compare>
        constexpr void introsort_loop(Iter first, Iter last, int depth_limit, Compare& cmp) noexcept(noexcept(cmp(*first, *first))) {
            constexpr dh::ptrdiff_t k_insertion_threshold = 16;
            while (last - first > k_insertion_threshold) {
                if (depth_limit == 0) {
                    dh::detail::heap_sort(first, last, cmp);
                    return;
                }
                --depth_limit;
                Iter split = dh::detail::partition(first, last, cmp);

                if (split - first < last - split) {
                    dh::detail::introsort_loop(first, split, depth_limit, cmp);
                    first = split;
                } else {
                    dh::detail::introsort_loop(split, last, depth_limit, cmp);
                    last = split;
                }
            }
            dh::detail::insertion_sort(first, last, cmp);
        }

    }

    template <typename Iter, typename Compare> constexpr void sort_by(Iter first, Iter last, Compare cmp) noexcept(noexcept(cmp(*first, *first))) {
        const dh::ptrdiff_t n = last - first;
        if (n < 2) {
            return;
        }
        int depth = 2 * detail::log2_floor(static_cast<dh::size_t>(n));
        detail::introsort_loop(first, last, depth, cmp);
    }

    template <typename Iter> constexpr void sort_by(Iter first, Iter last) noexcept(noexcept(*first < *first)) {
        sort_by(first, last, less{});
    }

    template <typename InputIt, typename T>
    [[nodiscard]] constexpr InputIt find(InputIt first, InputIt last, const T& value) noexcept(noexcept(*first == value)) {
        for (; first != last; ++first) {
            if (*first == value) {
                return first;
            }
        }
        return last;
    }

    template <typename InputIt, typename T>
    [[nodiscard]] constexpr dh::size_t count(InputIt first, InputIt last, const T& value) noexcept(noexcept(*first == value)) {
        dh::size_t n = 0;
        for (; first != last; ++first) {
            if (*first == value) {
                ++n;
            }
        }
        return n;
    }

    template <typename InputIt, typename OutputIt, typename UnaryOp>
    constexpr OutputIt transform(InputIt first, InputIt last, OutputIt out, UnaryOp op) noexcept(noexcept(*out = op(*first))) {
        for (; first != last; ++first, ++out) {
            *out = op(*first);
        }
        return out;
    }

    template <typename ForwardIt, typename Predicate>
    [[nodiscard]] constexpr ForwardIt remove_if(ForwardIt first, ForwardIt last, Predicate pred) noexcept(noexcept(pred(*first))) {
        ForwardIt out = first;
        for (; first != last; ++first) {
            if (!pred(*first)) {
                if (out != first) {
                    *out = dh::move(*first);
                }
                ++out;
            }
        }
        return out;
    }

    template <typename T, dh::size_t N> [[nodiscard]] constexpr dh::size_t size(const T (&)[N]) noexcept {
        return N;
    }

}
