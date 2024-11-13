// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh {

    using size_t = decltype(sizeof(int));
    using ptrdiff_t = decltype((char*)0 - (char*)0);
    using u8 = unsigned char;
    using u16 = unsigned short;
    using u32 = unsigned int;
    using u64 = unsigned long long;
    using i8 = signed char;
    using i16 = short;
    using i32 = int;
    using i64 = long long;

    template <typename T> [[nodiscard]] constexpr T* addressof(T& value) noexcept {
        return __builtin_addressof(value);
    }
    template <typename T> const T* addressof(const T&&) = delete;

    inline constexpr size_t k_unshared_bytes = 128;

    // initialize before constructing any dh container.
    [[nodiscard]] bool init_private_heap() noexcept;

    [[nodiscard]] void* allocate(size_t bytes, size_t alignment = 16) noexcept;

    template <typename T> [[nodiscard]] void* allocate_for() noexcept {
        constexpr size_t alignment = alignof(T) < alignof(void*) ? alignof(void*) : alignof(T);
        return allocate(sizeof(T), alignment);
    }

    [[noreturn]] void terminate() noexcept;

    [[noreturn]] void alloc_failed(const char* where) noexcept;

    using fail_fast_observer = void (*)(const char* reason, unsigned code) noexcept;
    void set_fail_fast_observer(fail_fast_observer observer) noexcept;
    [[noreturn]] void fail_fast(const char* reason, unsigned code) noexcept;

    void deallocate(void* ptr) noexcept;

    void memcpy(void* dst, const void* src, size_t bytes) noexcept;
    void memmove(void* dst, const void* src, size_t bytes) noexcept;
    void memset(void* dst, u8 value, size_t bytes) noexcept;
    [[nodiscard]] int memcmp(const void* a, const void* b, size_t bytes) noexcept;

    template <typename T> void copy_n(T* dst, const T* src, size_t count) noexcept {
        static_assert(__is_trivially_copyable(T), "dh::copy_n needs a trivially copyable type");
        dh::memcpy(dst, src, sizeof(T) * count);
    }

    template <typename T> [[nodiscard]] constexpr T* launder(T* p) noexcept {
        return __builtin_launder(p);
    }

}
