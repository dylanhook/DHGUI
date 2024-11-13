// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "format.h"

#include "from_chars.h"
#include "memory.h"
#include "numeric_limits.h"

namespace dh::detail {

    namespace {

        struct spec final {
            int positional{-1};
            int width{-1};
            int precision{-1};
            char type{0};
            bool zero_pad{false};
            bool hash_prefix{false};
            bool present{false};

            spec() noexcept = default;
            spec(const spec&) noexcept = default;
            spec(spec&&) noexcept = default;
            spec& operator=(const spec&) noexcept = default;
            spec& operator=(spec&&) noexcept = default;
            ~spec() = default;
        };

        struct token_buf final {
            char buf[96];
            unsigned int len{0};

            token_buf() noexcept : buf{}, len{0} {}

            token_buf(const token_buf&) noexcept = default;
            token_buf(token_buf&&) noexcept = default;
            token_buf& operator=(const token_buf&) noexcept = default;
            token_buf& operator=(token_buf&&) noexcept = default;
            ~token_buf() = default;

            void push(char c) noexcept {
                if (len < sizeof(buf)) {
                    buf[len++] = c;
                }
            }
        };

        enum class alignment : unsigned char {
            right,
            left,
        };

        constexpr int k_max_precision = 15;

        void reverse_token(token_buf& t) noexcept {
            for (unsigned int i = 0, j = t.len; i + 1 < j; ++i, --j) {
                const char tmp = t.buf[i];
                t.buf[i] = t.buf[j - 1];
                t.buf[j - 1] = tmp;
            }
        }

        [[nodiscard]] int parse_int(const char*& p, const char* end) noexcept {
            if (p >= end || *p < '0' || *p > '9') {
                return -1;
            }
            int v = 0;
            bool overflow = false;
            while (p < end && *p >= '0' && *p <= '9') {
                const int digit = *p - '0';
                if (v > (dh::numeric_limits<dh::i32>::max() - digit) / 10) overflow = true;
                if (!overflow) v = v * 10 + digit;
                ++p;
            }
            return overflow ? -2 : v;
        }

        [[nodiscard]] const char* parse_spec(const char* p, const char* end, spec& out) noexcept {
            out = spec{};

            const int idx = parse_int(p, end);
            if (idx == -2) return nullptr;
            if (idx >= 0) {
                out.positional = idx;
            }

            if (p < end && *p == ':') {
                out.present = true;
                ++p;

                while (p < end) {
                    if (*p == '#') {
                        out.hash_prefix = true;
                        ++p;
                    } else if (*p == '0') {
                        if (p + 1 < end && p[1] >= '0' && p[1] <= '9') {
                            out.zero_pad = true;
                            ++p;
                            break;
                        }
                        break;
                    } else {
                        break;
                    }
                }

                const int w = parse_int(p, end);
                if (w == -2) return nullptr;
                if (w >= 0) {
                    out.width = w;
                }

                if (p < end && *p == '.') {
                    ++p;
                    const int prec = parse_int(p, end);
                    if (prec < 0) {
                        return nullptr;
                    }
                    if (prec > k_max_precision) {
                        return nullptr;
                    }
                    out.precision = prec;
                }

                if (p < end && *p != '}') {
                    const char c = *p;
                    switch (c) {
                        case 'd':
                        case 'x':
                        case 'X':
                        case 'b':
                        case 'o':
                        case 'c':
                        case 's':
                        case 'p':
                        case 'f':
                        case 'g':
                        case 'G':
                            out.type = c;
                            ++p;
                            break;
                        default:
                            return nullptr;
                    }
                }
            }

            if (p >= end || *p != '}') {
                return nullptr;
            }
            return p;
        }

        void u64_to_decimal(unsigned long long v, token_buf& t) noexcept {
            if (v == 0) {
                t.push('0');
                return;
            }
            while (v) {
                t.push(static_cast<char>('0' + (v % 10)));
                v /= 10;
            }
            reverse_token(t);
        }

        void i64_to_decimal(long long v, token_buf& t) noexcept {
            if (v < 0) {
                const unsigned long long mag = static_cast<unsigned long long>(0) - static_cast<unsigned long long>(v);
                t.push('-');

                token_buf digits{};
                u64_to_decimal(mag, digits);
                for (unsigned int i = 0; i < digits.len; ++i) {
                    t.push(digits.buf[i]);
                }
            } else {
                u64_to_decimal(static_cast<unsigned long long>(v), t);
            }
        }

        void u64_to_hex(unsigned long long v, token_buf& t, bool upper) noexcept {
            static constexpr char k_lower[] = "0123456789abcdef";
            static constexpr char k_upper[] = "0123456789ABCDEF";
            const char* alphabet = upper ? k_upper : k_lower;
            if (v == 0) {
                t.push('0');
                return;
            }
            while (v) {
                t.push(alphabet[v & 0xF]);
                v >>= 4;
            }
            reverse_token(t);
        }

        void u64_to_binary(unsigned long long v, token_buf& t) noexcept {
            if (v == 0) {
                t.push('0');
                return;
            }
            while (v) {
                t.push(static_cast<char>('0' + (v & 1)));
                v >>= 1;
            }
            reverse_token(t);
        }

        void u64_to_octal(unsigned long long v, token_buf& t) noexcept {
            if (v == 0) {
                t.push('0');
                return;
            }
            while (v) {
                t.push(static_cast<char>('0' + (v & 7)));
                v >>= 3;
            }
            reverse_token(t);
        }

        constexpr unsigned long long k_pow10[16] = {
            1ULL,
            10ULL,
            100ULL,
            1000ULL,
            10000ULL,
            100000ULL,
            1000000ULL,
            10000000ULL,
            100000000ULL,
            1000000000ULL,
            10000000000ULL,
            100000000000ULL,
            1000000000000ULL,
            10000000000000ULL,
            100000000000000ULL,
            1000000000000000ULL,
        };

        static_assert(sizeof(k_pow10) / sizeof(k_pow10[0]) == static_cast<dh::size_t>(k_max_precision) + 1, "k_max_precision must match the last k_pow10 entry");

        constexpr double k_u64_cast_ceiling = 18446744073709551616.0;

        constexpr int k_default_precision = 6;

        void f64_to_decimal(double v, int precision, token_buf& t) noexcept {
            if (precision < 0) {
                precision = k_default_precision;
            }

            if (precision > k_max_precision) {
                precision = k_max_precision;
            }

            unsigned long long bits = 0;
            dh::memcpy(&bits, &v, sizeof(bits));
            const unsigned long long sign_bit = bits >> 63;
            const unsigned long long exponent = (bits >> 52) & 0x7FFULL;
            const unsigned long long mantissa = bits & 0xFFFFFFFFFFFFFULL;

            if (exponent == 0x7FF) {
                if (sign_bit) {
                    t.push('-');
                }
                if (mantissa == 0) {
                    t.push('i');
                    t.push('n');
                    t.push('f');
                } else {
                    t.push('n');
                    t.push('a');
                    t.push('n');
                }
                return;
            }

            if (sign_bit) {
                t.push('-');
                v = -v;
            }

            const unsigned long long scale = k_pow10[precision];
            const double scaled = v * static_cast<double>(scale) + 0.5;

            if (!(scaled < k_u64_cast_ceiling)) {
                t.push('o');
                t.push('v');
                t.push('f');
                return;
            }
            const unsigned long long whole_part = static_cast<unsigned long long>(scaled);

            const unsigned long long int_part = whole_part / scale;
            const unsigned long long frac_part = whole_part % scale;

            token_buf int_digits{};
            u64_to_decimal(int_part, int_digits);
            for (unsigned int i = 0; i < int_digits.len; ++i) {
                t.push(int_digits.buf[i]);
            }

            if (precision == 0) {
                return;
            }

            t.push('.');

            token_buf frac_digits{};
            u64_to_decimal(frac_part, frac_digits);
            for (int pad = precision - static_cast<int>(frac_digits.len); pad > 0; --pad) {
                t.push('0');
            }
            for (unsigned int i = 0; i < frac_digits.len; ++i) {
                t.push(frac_digits.buf[i]);
            }
        }

        void f32_to_round_trip(float value, token_buf& token) noexcept {
            constexpr int k_significant_digits = 9;
            constexpr int k_min_plain_exponent = -4;
            double magnitude = static_cast<double>(value);
            dh::u64 bits = 0;
            dh::memcpy(&bits, &magnitude, sizeof(bits));
            if (((bits >> 52) & 0x7FFULL) == 0x7FFULL || magnitude == 0.0) {
                f64_to_decimal(magnitude, 0, token);
                return;
            }
            if ((bits >> 63) != 0) {
                token.push('-');
                magnitude = -magnitude;
            }

            int exponent = 0;
            while (magnitude >= 10.0) {
                magnitude /= 10.0;
                ++exponent;
            }
            while (magnitude < 1.0) {
                magnitude *= 10.0;
                --exponent;
            }
            dh::u64 coefficient = static_cast<dh::u64>(magnitude * static_cast<double>(k_pow10[k_significant_digits - 1]) + 0.5);
            if (coefficient == k_pow10[k_significant_digits]) {
                coefficient /= 10;
                ++exponent;
            }
            token_buf digits{};
            u64_to_decimal(coefficient, digits);
            while (digits.len > 1 && digits.buf[digits.len - 1] == '0') --digits.len;

            const bool scientific = exponent < k_min_plain_exponent || exponent >= k_significant_digits;
            const int point = scientific ? 1 : exponent + 1;
            if (point <= 0) {
                token.push('0');
                token.push('.');
                for (int index = 0; index < -point; ++index) token.push('0');
            }
            for (unsigned int index = 0; index < digits.len; ++index) {
                if (index != 0 && static_cast<int>(index) == point) token.push('.');
                token.push(digits.buf[index]);
            }
            for (int index = static_cast<int>(digits.len); index < point; ++index) token.push('0');
            if (scientific) {
                token.push('e');
                token_buf exponent_digits{};
                i64_to_decimal(exponent, exponent_digits);
                for (unsigned int index = 0; index < exponent_digits.len; ++index) token.push(exponent_digits.buf[index]);
            }
        }

        void pointer_to_text(const void* p, token_buf& t) noexcept {
            t.push('0');
            t.push('x');
            const unsigned long long bits = reinterpret_cast<unsigned long long>(p);
            token_buf digits{};
            u64_to_hex(bits, digits, false);
            for (int pad = 16 - static_cast<int>(digits.len); pad > 0; --pad) {
                t.push('0');
            }
            for (unsigned int i = 0; i < digits.len; ++i) {
                t.push(digits.buf[i]);
            }
        }

        void append_padded(dh::string& out, const char* s, unsigned long long len, int width, char fill, alignment align) noexcept {
            const int pad = width > 0 && static_cast<unsigned long long>(width) > len ? width - static_cast<int>(len) : 0;
            if (align == alignment::right) {
                for (int i = 0; i < pad; ++i) {
                    out.push_back(fill);
                }
            }
            out.append(s, static_cast<unsigned long long>(len));
            if (align == alignment::left) {
                for (int i = 0; i < pad; ++i) {
                    out.push_back(fill);
                }
            }
        }

        void append_kind_mismatch(dh::string& out, const spec& s) noexcept {
            append_padded(out, "(kind)", 6, s.width, ' ', alignment::left);
        }

        void emit_integer_token(dh::string& out, const token_buf& body, const char* prefix, unsigned long long prefix_len, int width, bool zero_pad) noexcept {
            const int body_len = static_cast<int>(body.len);
            const int total_len = static_cast<int>(prefix_len) + body_len;
            const int pad = (width > total_len) ? (width - total_len) : 0;
            const char fill = zero_pad ? '0' : ' ';

            if (zero_pad) {
                out.append(prefix, prefix_len);
                for (int i = 0; i < pad; ++i) {
                    out.push_back(fill);
                }
                out.append(body.buf, static_cast<unsigned long long>(body_len));
            } else {
                for (int i = 0; i < pad; ++i) {
                    out.push_back(fill);
                }
                out.append(prefix, prefix_len);
                out.append(body.buf, static_cast<unsigned long long>(body_len));
            }
        }

        void emit_arg(dh::string& out, const arg_value& a, const spec& s) noexcept {
            char t = s.type;
            if (t == 0) {
                switch (a.kind) {
                    case arg_kind::i64:
                        t = 'd';
                        break;
                    case arg_kind::u64:
                        t = 'd';
                        break;
                    case arg_kind::f64:
                        t = 'f';
                        break;
                    case arg_kind::character:
                        t = 'c';
                        break;
                    case arg_kind::c_string:
                        t = 's';
                        break;
                    case arg_kind::string_v:
                        t = 's';
                        break;
                    case arg_kind::pointer:
                        t = 'p';
                        break;
                    case arg_kind::boolean:
                        t = 'd';
                        break;
                    case arg_kind::none:
                        return;
                }
            }

            switch (t) {
                case 'd': {
                    token_buf body{};
                    if (a.kind == arg_kind::i64) {
                        i64_to_decimal(a.i64_value, body);
                    } else if (a.kind == arg_kind::u64) {
                        u64_to_decimal(a.u64_value, body);
                    } else if (a.kind == arg_kind::boolean) {
                        u64_to_decimal(a.bool_value ? 1ULL : 0ULL, body);
                    } else if (a.kind == arg_kind::character) {
                        u64_to_decimal(static_cast<unsigned long long>(static_cast<unsigned char>(a.char_value)), body);
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    emit_integer_token(out, body, nullptr, 0, s.width, s.zero_pad);
                    break;
                }
                case 'x':
                case 'X': {
                    token_buf body{};
                    unsigned long long v = 0;
                    if (a.kind == arg_kind::u64) {
                        v = a.u64_value;
                    } else if (a.kind == arg_kind::i64) {
                        v = static_cast<unsigned long long>(a.i64_value);
                    } else if (a.kind == arg_kind::pointer) {
                        v = reinterpret_cast<unsigned long long>(a.ptr_value);
                    } else if (a.kind == arg_kind::boolean) {
                        v = a.bool_value ? 1ULL : 0ULL;
                    } else if (a.kind == arg_kind::character) {
                        v = static_cast<unsigned long long>(static_cast<unsigned char>(a.char_value));
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    u64_to_hex(v, body, t == 'X');
                    const char* prefix = nullptr;
                    unsigned long long prefix_len = 0;
                    if (s.hash_prefix) {
                        prefix = (t == 'X') ? "0X" : "0x";
                        prefix_len = 2;
                    }
                    emit_integer_token(out, body, prefix, prefix_len, s.width, s.zero_pad);
                    break;
                }
                case 'b': {
                    token_buf body{};
                    unsigned long long v = 0;
                    if (a.kind == arg_kind::u64) {
                        v = a.u64_value;
                    } else if (a.kind == arg_kind::i64) {
                        v = static_cast<unsigned long long>(a.i64_value);
                    } else if (a.kind == arg_kind::boolean) {
                        v = a.bool_value ? 1ULL : 0ULL;
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    u64_to_binary(v, body);
                    emit_integer_token(out, body, nullptr, 0, s.width, s.zero_pad);
                    break;
                }
                case 'o': {
                    token_buf body{};
                    unsigned long long v = 0;
                    if (a.kind == arg_kind::u64) {
                        v = a.u64_value;
                    } else if (a.kind == arg_kind::i64) {
                        v = static_cast<unsigned long long>(a.i64_value);
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    u64_to_octal(v, body);
                    emit_integer_token(out, body, nullptr, 0, s.width, s.zero_pad);
                    break;
                }
                case 'c': {
                    char c = 0;
                    if (a.kind == arg_kind::character) {
                        c = a.char_value;
                    } else if (a.kind == arg_kind::i64) {
                        c = static_cast<char>(a.i64_value);
                    } else if (a.kind == arg_kind::u64) {
                        c = static_cast<char>(a.u64_value);
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    append_padded(out, &c, 1, s.width, ' ', alignment::right);
                    break;
                }
                case 's': {
                    const char* data = nullptr;
                    unsigned long long size = 0;
                    if (a.kind == arg_kind::string_v) {
                        data = a.sv_value.data;
                        size = a.sv_value.size;
                    } else if (a.kind == arg_kind::c_string) {
                        data = a.cstr_value;
                        if (data) {
                            size = static_cast<unsigned long long>(dh::strlen(data));
                        }
                    } else if (a.kind == arg_kind::character) {
                        append_padded(out, &a.char_value, 1, s.width, ' ', alignment::left);
                        return;
                    } else if (a.kind == arg_kind::boolean) {
                        if (a.bool_value) {
                            append_padded(out, "true", 4, s.width, ' ', alignment::left);
                        } else {
                            append_padded(out, "false", 5, s.width, ' ', alignment::left);
                        }
                        return;
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }

                    const bool null_is_a_bug = data == nullptr && (size != 0 || a.kind == arg_kind::c_string);
                    if (null_is_a_bug) {
                        append_padded(out, "(null)", 6, s.width, ' ', alignment::left);
                    } else if (size == 0) {
                        append_padded(out, "", 0, s.width, ' ', alignment::left);
                    } else {
                        append_padded(out, data, size, s.width, ' ', alignment::left);
                    }
                    break;
                }
                case 'p': {
                    token_buf body{};
                    const void* p = nullptr;
                    if (a.kind == arg_kind::pointer) {
                        p = a.ptr_value;
                    } else if (a.kind == arg_kind::c_string) {
                        p = a.cstr_value;
                    } else if (a.kind == arg_kind::u64) {
                        p = reinterpret_cast<const void*>(static_cast<unsigned long long>(a.u64_value));
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    pointer_to_text(p, body);

                    append_padded(out, body.buf, body.len, s.width, ' ', alignment::right);
                    break;
                }
                case 'f': {
                    token_buf body{};
                    double v = 0.0;
                    if (a.kind == arg_kind::f64) {
                        v = a.f64_value;
                    } else if (a.kind == arg_kind::i64) {
                        v = static_cast<double>(a.i64_value);
                    } else if (a.kind == arg_kind::u64) {
                        v = static_cast<double>(a.u64_value);
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    f64_to_decimal(v, s.precision, body);

                    append_padded(out, body.buf, body.len, s.width, ' ', alignment::right);
                    break;
                }
                case 'g':
                case 'G': {
                    token_buf body{};
                    double v = 0.0;
                    if (a.kind == arg_kind::f64) {
                        v = a.f64_value;
                    } else if (a.kind == arg_kind::i64) {
                        v = static_cast<double>(a.i64_value);
                    } else if (a.kind == arg_kind::u64) {
                        v = static_cast<double>(a.u64_value);
                    } else {
                        append_kind_mismatch(out, s);
                        return;
                    }
                    f64_to_decimal(v, s.precision, body);

                    bool has_dot = false;
                    for (unsigned int i = 0; i < body.len; ++i) {
                        if (body.buf[i] == '.') {
                            has_dot = true;
                            break;
                        }
                    }
                    if (has_dot) {
                        while (body.len > 0 && body.buf[body.len - 1] == '0') {
                            --body.len;
                        }
                        if (body.len > 0 && body.buf[body.len - 1] == '.') {
                            --body.len;
                        }
                    }
                    append_padded(out, body.buf, body.len, s.width, ' ', alignment::right);
                    break;
                }
                default:

                    append_padded(out, "(spec)", 6, s.width, ' ', alignment::left);
                    break;
            }
        }

    }

    dh::expected<number_format_field, number_format_error> validate_number_format_impl(dh::string_view format, dh::size_t maximum_width) noexcept {
        const char* const begin = format.data();
        const char* const end = begin + format.size();
        number_format_field field;
        for (const char* cursor = begin; cursor < end; ++cursor) {
            if ((*cursor == '{' || *cursor == '}') && cursor + 1 < end && cursor[1] == *cursor) { ++cursor; continue; }
            if (*cursor == '}') return dh::unexpected{number_format_error::malformed};
            if (*cursor != '{') continue;
            if (field.length != 0) return dh::unexpected{number_format_error::field_count};
            spec parsed;
            const char* const close = parse_spec(cursor + 1, end, parsed);
            if (close == nullptr) return dh::unexpected{number_format_error::malformed};
            if (parsed.positional > 0) return dh::unexpected{number_format_error::argument_index};
            if (parsed.type != 0 && parsed.type != 'f' && parsed.type != 'g' && parsed.type != 'G')
                return dh::unexpected{number_format_error::type_mismatch};
            if (parsed.width >= 0 && static_cast<dh::size_t>(parsed.width) > maximum_width)
                return dh::unexpected{number_format_error::width_limit};
            field.offset = static_cast<dh::size_t>(cursor - begin);
            field.length = static_cast<dh::size_t>(close - cursor + 1);
            cursor = close;
        }
        if (field.length == 0) return dh::unexpected{number_format_error::field_count};
        return field;
    }

    void format_engine(dh::string& out, dh::string_view fmt, const arg_value* args, unsigned long long n) noexcept {
        const char* p = fmt.data();
        const char* end = p + fmt.size();
        unsigned long long auto_index = 0;

        while (p < end) {
            const char c = *p;

            if (c == '{' && (p + 1) < end && p[1] == '{') {
                out.push_back('{');
                p += 2;
                continue;
            }
            if (c == '}' && (p + 1) < end && p[1] == '}') {
                out.push_back('}');
                p += 2;
                continue;
            }

            if (c != '{') {
                out.push_back(c);
                ++p;
                continue;
            }

            const char* spec_start = p;
            spec s{};
            const char* close = parse_spec(p + 1, end, s);
            if (close == nullptr) {
                const char* scan = p;
                while (scan < end && *scan != '}') {
                    ++scan;
                }
                if (scan < end) {
                    ++scan;
                }
                out.append(spec_start, static_cast<unsigned long long>(scan - spec_start));
                p = scan;
                continue;
            }

            unsigned long long idx = 0;
            if (s.positional >= 0) {
                idx = static_cast<unsigned long long>(s.positional);
            } else {
                idx = auto_index++;
            }

            if (idx < n) {
                emit_arg(out, args[idx], s);
            } else {
                out.append(spec_start, static_cast<unsigned long long>(close + 1 - spec_start));
            }

            p = close + 1;
        }
    }

}

namespace dh {

#pragma fenv_access(on)
    void append_float_round_trip(dh::string& out, float value) noexcept {
        detail::token_buf token{};
        {
            const detail::number_conversion_environment environment;
            detail::f32_to_round_trip(value, token);
        }
        out.append(token.buf, token.len);
    }
#pragma fenv_access(off)

    dh::string_view to_string(number_format_error error) noexcept {
        switch (error) {
            case number_format_error::malformed: return "malformed_number_format";
            case number_format_error::field_count: return "number_format_requires_one_field";
            case number_format_error::argument_index: return "number_format_argument_must_be_zero";
            case number_format_error::type_mismatch: return "number_format_requires_floating_point_type";
            case number_format_error::width_limit: return "number_format_width_exceeds_limit";
        }
        return "unknown";
    }

    dh::expected<number_format_field, number_format_error> validate_number_format(dh::string_view format, dh::size_t maximum_width) noexcept {
        return detail::validate_number_format_impl(format, maximum_width);
    }

}
