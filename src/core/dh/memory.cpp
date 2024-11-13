// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "core/dh/memory.h"
#include "core/dh/pch_contract.h"

#pragma intrinsic(memcpy, memmove, memset, memcmp)

#include <stdlib.h>
#include <new>
#include "core/dh/atomic.h"

namespace dh {

    namespace {

        HANDLE g_private_heap = nullptr;

        struct alloc_header final {
            void* raw_ptr;
        };

        [[nodiscard]] inline void* zero_sentinel() noexcept {
            return reinterpret_cast<void*>(static_cast<dh::u64>(1));
        }

        [[nodiscard]] inline bool is_sentinel(const void* p) noexcept {
            return p == zero_sentinel();
        }

        [[nodiscard]] constexpr dh::u64 align_up(dh::u64 value, size_t alignment) noexcept {
            const auto mask = static_cast<dh::u64>(alignment) - 1ULL;
            return (value + mask) & ~mask;
        }

        [[nodiscard]] unsigned char* header_bytes(void* user_ptr) noexcept {
            return static_cast<unsigned char*>(user_ptr) - sizeof(alloc_header);
        }

    }

    bool init_private_heap() noexcept {
        if (g_private_heap != nullptr) return true;

        HANDLE const heap = ::HeapCreate(0u, 0u, 0u);
        if (heap == nullptr) {
#if !DH_LOG_COMPILED_OUT
            ::OutputDebugStringA("dh: HeapCreate failed\n");
#endif
            return false;
        }
        g_private_heap = heap;
        return true;
    }

    void terminate() noexcept {
        ::abort();
    }

    namespace {

        constinit dh::atomic<fail_fast_observer> g_fail_fast_observer{nullptr};
    }

    void set_fail_fast_observer(fail_fast_observer observer) noexcept {
        g_fail_fast_observer.store(observer, dh::memory_order_release);
    }

    void fail_fast(const char* reason, unsigned code) noexcept {
        if (const auto observer = g_fail_fast_observer.load(dh::memory_order_acquire); observer != nullptr) {
            observer(reason, code);
        }
        __fastfail(code);
    }

    void alloc_failed(const char* where) noexcept {
        fail_fast(where, FAST_FAIL_FATAL_APP_EXIT);
    }

    void* allocate(size_t bytes, size_t alignment) noexcept {
        if (bytes == 0) return zero_sentinel();

        HANDLE const heap = g_private_heap;
        if (heap == nullptr) return nullptr;

        constexpr size_t maximum = static_cast<size_t>(-1);
        if (alignment < alignof(void*) || (alignment & (alignment - 1)) != 0 || alignment - 1 > maximum - sizeof(alloc_header)) {
            return nullptr;
        }
        const size_t overhead = alignment - 1 + sizeof(alloc_header);
        if (bytes > maximum - overhead) return nullptr;
        const size_t total = bytes + overhead;

        auto* const raw = ::HeapAlloc(heap, HEAP_ZERO_MEMORY, total);
        if (raw == nullptr) return nullptr;

        const auto raw_addr = reinterpret_cast<dh::u64>(raw);
        const auto user_addr = align_up(raw_addr + sizeof(alloc_header), alignment);
        auto* const user_ptr = reinterpret_cast<void*>(user_addr);

        const alloc_header header{raw};
        dh::memcpy(header_bytes(user_ptr), &header, sizeof(header));
        return user_ptr;
    }

    void deallocate(void* ptr) noexcept {
        if (ptr == nullptr || is_sentinel(ptr)) return;

        alloc_header header{};
        dh::memcpy(&header, header_bytes(ptr), sizeof(header));
        ::HeapFree(g_private_heap, 0, header.raw_ptr);
    }

    void memcpy(void* dst, const void* src, size_t bytes) noexcept {
        ::memcpy(dst, src, bytes);
    }

    void memmove(void* dst, const void* src, size_t bytes) noexcept {
        ::memmove(dst, src, bytes);
    }

    void memset(void* dst, u8 value, size_t bytes) noexcept {
        ::memset(dst, static_cast<int>(value), bytes);
    }

    int memcmp(const void* a, const void* b, size_t bytes) noexcept {
        return ::memcmp(a, b, bytes);
    }

}

void* operator new(dh::size_t bytes) {
    void* const p = dh::allocate(bytes == 0 ? 1 : bytes);
    if (p == nullptr) {
        dh::alloc_failed("operator new");
    }
    return p;
}

void* operator new[](dh::size_t bytes) {
    void* const p = dh::allocate(bytes == 0 ? 1 : bytes);
    if (p == nullptr) {
        dh::alloc_failed("operator new[]");
    }
    return p;
}

void operator delete(void* ptr) noexcept {
    dh::deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
    dh::deallocate(ptr);
}

void operator delete(void* ptr, dh::size_t) noexcept {
    dh::deallocate(ptr);
}

void operator delete[](void* ptr, dh::size_t) noexcept {
    dh::deallocate(ptr);
}

void* operator new(dh::size_t bytes, std::align_val_t alignment) {
    void* const p = dh::allocate(bytes == 0 ? 1 : bytes, static_cast<dh::size_t>(alignment));
    if (p == nullptr) dh::alloc_failed("aligned operator new");
    return p;
}

void* operator new[](dh::size_t bytes, std::align_val_t alignment) {
    return ::operator new(bytes, alignment);
}

void operator delete(void* ptr, std::align_val_t) noexcept {
    dh::deallocate(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept {
    dh::deallocate(ptr);
}

void operator delete(void* ptr, dh::size_t, std::align_val_t) noexcept {
    dh::deallocate(ptr);
}

void operator delete[](void* ptr, dh::size_t, std::align_val_t) noexcept {
    dh::deallocate(ptr);
}
