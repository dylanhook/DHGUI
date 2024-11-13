// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "string.h"
#include "expected.h"

#include <cstdio>
#include <cwchar>
#include "core/dh/type_traits.h"

namespace dh {

    enum class number_format_error : unsigned char { malformed, field_count, argument_index, type_mismatch, width_limit };
    [[nodiscard]] dh::string_view to_string(number_format_error error) noexcept;
    struct number_format_field final {
    public:
        number_format_field() noexcept = default;
        ~number_format_field() = default;
        number_format_field(const number_format_field&) = default;
        number_format_field& operator=(const number_format_field&) = default;
        number_format_field(number_format_field&&) = default;
        number_format_field& operator=(number_format_field&&) = default;
        dh::size_t offset{0};
        dh::size_t length{0};
    };

    [[nodiscard]] dh::expected<number_format_field, number_format_error> validate_number_format(dh::string_view format, dh::size_t maximum_width) noexcept;

    void append_float_round_trip(dh::string& out, float value) noexcept;

    template <typename... Args> [[nodiscard]] inline int snprintf(char* buf, dh::size_t n, const char* fmt, Args... args) noexcept {
        return ::snprintf(buf, n, fmt, args...);
    }

    template <typename... Args> [[nodiscard]] inline int swprintf(wchar_t* buf, dh::size_t n, const wchar_t* fmt, Args... args) noexcept {
        return ::swprintf(buf, n, fmt, args...);
    }

    enum class arg_kind : unsigned char {
        none,
        i64,
        u64,
        f64,
        boolean,
        character,
        c_string,
        string_v,
        pointer,
    };

    struct arg_value final {
        arg_kind kind{arg_kind::none};

        union {
            long long i64_value;
            unsigned long long u64_value;
            double f64_value;
            bool bool_value;
            char char_value;
            const char* cstr_value;
            const void* ptr_value;

            struct {
                const char* data;
                unsigned long long size;
            } sv_value;
        };

        constexpr arg_value() noexcept : kind(arg_kind::none), u64_value(0) {}

        arg_value(const arg_value&) noexcept = default;
        arg_value(arg_value&&) noexcept = default;
        arg_value& operator=(const arg_value&) noexcept = default;
        arg_value& operator=(arg_value&&) noexcept = default;
        ~arg_value() = default;
    };

    namespace detail {

        template <typename T> constexpr auto detect_string_like(int) -> decltype(static_cast<const char*>(dh::declval<const T&>().data()), static_cast<unsigned long long>(dh::declval<const T&>().size()), dh::true_type{});

        template <typename T> constexpr dh::false_type detect_string_like(...);

        template <typename T> inline constexpr bool is_string_like_v = decltype(detect_string_like<T>(0))::value;

        template <typename> inline constexpr bool always_false_v = false;

        template <typename T> [[nodiscard]] constexpr arg_value capture(const T& v) noexcept {
            arg_value a{};
            using clean = dh::remove_cv_t<dh::remove_reference_t<T>>;

            if constexpr (dh::is_same_v<clean, bool>) {
                a.kind = arg_kind::boolean;
                a.bool_value = v;
            } else if constexpr (dh::is_same_v<clean, char>) {
                a.kind = arg_kind::character;
                a.char_value = v;
            } else if constexpr (dh::is_integral_v<clean> && dh::is_signed_v<clean>) {
                a.kind = arg_kind::i64;
                a.i64_value = static_cast<long long>(v);
            } else if constexpr (dh::is_integral_v<clean> && dh::is_unsigned_v<clean>) {
                a.kind = arg_kind::u64;
                a.u64_value = static_cast<unsigned long long>(v);
            } else if constexpr (dh::is_floating_point_v<clean>) {
                a.kind = arg_kind::f64;
                a.f64_value = static_cast<double>(v);
            } else if constexpr (is_string_like_v<clean>) {
                a.kind = arg_kind::string_v;
                a.sv_value.data = v.data();
                a.sv_value.size = static_cast<unsigned long long>(v.size());
            } else if constexpr (dh::is_pointer_v<clean>) {
                using pointee = dh::remove_cv_t<dh::remove_pointer_t<clean>>;
                if constexpr (dh::is_same_v<pointee, char>) {
                    a.kind = arg_kind::c_string;
                    a.cstr_value = reinterpret_cast<const char*>(v);
                } else {
                    a.kind = arg_kind::pointer;
                    a.ptr_value = reinterpret_cast<const void*>(v);
                }
            } else if constexpr (dh::is_array_v<clean> && dh::is_same_v<dh::remove_cv_t<dh::remove_extent_t<clean>>, char>) {
                a.kind = arg_kind::c_string;
                a.cstr_value = static_cast<const char*>(v);
            } else {
                static_assert(always_false_v<clean>, "dh::format cannot use this type, add it to capture() or convert it first");
            }
            return a;
        }

        void format_engine(dh::string& out, dh::string_view fmt, const arg_value* args, unsigned long long n) noexcept;

    }

    template <typename... Args> [[nodiscard]] dh::string format(dh::string_view fmt, const Args&... args) noexcept {
        dh::string out;
        if constexpr (sizeof...(Args) == 0) {
            detail::format_engine(out, fmt, nullptr, 0);
        } else {
            const arg_value packed[] = {detail::capture(args)...};
            detail::format_engine(out, fmt, packed, sizeof...(Args));
        }
        return out;
    }

    template <typename... Args> void format_to(dh::string& out, dh::string_view fmt, const Args&... args) noexcept {
        if constexpr (sizeof...(Args) == 0) {
            detail::format_engine(out, fmt, nullptr, 0);
        } else {
            const arg_value packed[] = {detail::capture(args)...};
            detail::format_engine(out, fmt, packed, sizeof...(Args));
        }
    }

}
