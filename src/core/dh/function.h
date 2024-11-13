// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "memory.h"
#include "unique_ptr.h"

#include <new>
#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh {

    namespace detail {

        inline constexpr dh::size_t k_function_sbo_bytes = 32;
        inline constexpr dh::size_t k_function_sbo_align = 16;

        template <typename R, typename... Args> struct function_ops final {
            R (*invoke)(void* callable, Args... args);
            void (*copy)(void* dst_storage, const void* src);
            void (*move)(void* dst_storage, void* src) noexcept;
            void (*destroy)(void* callable) noexcept;
            bool stored_inline;
        };

        template <typename F> inline constexpr bool fits_inline_v =
            sizeof(F) <= k_function_sbo_bytes && alignof(F) <= k_function_sbo_align && dh::is_nothrow_move_constructible_v<F>;

        struct spill_deleter final {
            void operator()(dh::u8* bytes) const noexcept { dh::deallocate(bytes); }
        };

        using spill_owner = dh::unique_ptr<dh::u8, spill_deleter>;

        template <typename F> [[nodiscard]] inline spill_owner allocate_spill(const char* where) noexcept {
            constexpr dh::size_t alignment = alignof(F) < alignof(void*) ? alignof(void*) : alignof(F);
            void* fresh = dh::allocate(sizeof(F), alignment);
            if (fresh == nullptr) {
                dh::alloc_failed(where);
            }
            return spill_owner{static_cast<dh::u8*>(fresh)};
        }

        template <typename F, typename R, typename... Args> struct callable_thunks final {
            static R invoke(void* callable, Args... args) {
                F* target = static_cast<F*>(callable);
                if constexpr (dh::is_void_v<R>) {
                    (*target)(dh::forward<Args>(args)...);
                } else {
                    return (*target)(dh::forward<Args>(args)...);
                }
            }

            static void move(void* dst_storage, void* src) noexcept {
                F* source = static_cast<F*>(src);
                if constexpr (fits_inline_v<F>) {
                    ::new (dst_storage) F(static_cast<F&&>(*source));

                    source->~F();
                } else {
                    *static_cast<void**>(dst_storage) = source;
                }
            }

            static void destroy(void* callable) noexcept {
                F* target = static_cast<F*>(callable);
                target->~F();
                if constexpr (!fits_inline_v<F>) {
                    dh::deallocate(target);
                }
            }
        };

        template <typename F> struct copy_thunk final {
            static void apply(void* dst_storage, const void* src) {
                const F* source = static_cast<const F*>(src);
                if constexpr (fits_inline_v<F>) {
                    ::new (dst_storage) F(*source);
                } else {
                    auto fresh = allocate_spill<F>("dh::function::copy");
                    ::new (static_cast<void*>(fresh.get())) F(*source);

                    *static_cast<void**>(dst_storage) = fresh.release();
                }
            }
        };

        template <typename F, typename R, typename... Args> [[nodiscard]] inline const function_ops<R, Args...>* ops_for() noexcept {
            using thunks = callable_thunks<F, R, Args...>;
            static constexpr function_ops<R, Args...> table{
                &thunks::invoke,
                &copy_thunk<F>::apply,
                &thunks::move,
                &thunks::destroy,
                fits_inline_v<F>,
            };
            return &table;
        }

        [[noreturn]] inline void empty_call() noexcept {
            dh::fail_fast("call through an empty dh::function", FAST_FAIL_INVALID_ARG);
        }

        template <typename R, typename... Args> class function_body final {
        public:
            using ops_table = function_ops<R, Args...>;

            function_body() noexcept = default;

            ~function_body() noexcept { clear(); }

            function_body(const function_body&) = delete;
            function_body& operator=(const function_body&) = delete;
            function_body(function_body&&) = delete;
            function_body& operator=(function_body&&) = delete;

            [[nodiscard]] bool holds_callable() const noexcept { return m_ops != nullptr; }

            template <typename F, typename Arg> void construct(Arg&& source) noexcept(dh::is_nothrow_constructible_v<F, Arg&&>) {
                if constexpr (fits_inline_v<F>) {
                    ::new (&m_storage) F(dh::forward<Arg>(source));
                } else {
                    auto fresh = allocate_spill<F>("dh::function::construct");
                    ::new (static_cast<void*>(fresh.get())) F(dh::forward<Arg>(source));
                    m_storage.heap_ptr = fresh.release();
                }
                m_ops = ops_for<F, R, Args...>();
            }

            void adopt(function_body& source) noexcept {
                if (source.m_ops == nullptr) {
                    return;
                }
                source.m_ops->move(&m_storage, source.target_pointer());
                m_ops = source.m_ops;
                if (!source.m_ops->stored_inline) {
                    source.m_storage.heap_ptr = nullptr;
                }
                source.reset_metadata();
            }

            void clone(const function_body& source) {
                if (source.m_ops == nullptr) {
                    return;
                }
                source.m_ops->copy(&m_storage, source.target_pointer());
                m_ops = source.m_ops;
            }

            template <typename... CallArgs> R invoke(CallArgs&&... args) const {
                if (m_ops == nullptr) {
                    empty_call();
                }
                return m_ops->invoke(target_pointer(), dh::forward<CallArgs>(args)...);
            }

            void clear() noexcept {
                if (m_ops == nullptr) {
                    return;
                }
                m_ops->destroy(target_pointer());
                reset_metadata();
            }

        private:
            [[nodiscard]] void* target_pointer() const noexcept {
                if (m_ops == nullptr) {
                    return nullptr;
                }
                if (m_ops->stored_inline) {
                    return const_cast<void*>(static_cast<const void*>(&m_storage));
                }
                return m_storage.heap_ptr;
            }

            void reset_metadata() noexcept {
                m_ops = nullptr;
                m_storage.heap_ptr = nullptr;
            }

            union storage_union {
                alignas(k_function_sbo_align) unsigned char inline_buffer[k_function_sbo_bytes];
                void* heap_ptr;
            };

            static_assert(sizeof(storage_union) == k_function_sbo_bytes, "function storage must match the inline buffer size");

            storage_union m_storage{};
            const ops_table* m_ops{nullptr};
        };

    }

    template <typename Signature> class function;

    template <typename R, typename... Args> class function<R(Args...)> final {
    public:
        using result_type = R;

        function() noexcept = default;

        function(decltype(nullptr)) noexcept {}

        template <typename F,
            typename = dh::enable_if_t<!dh::is_same_v<dh::remove_cvref_t<F>, function> && dh::is_invocable_r_v<R, dh::remove_reference_t<F>&, Args...> &&
                dh::is_copy_constructible_v<dh::decay_t<F>>>>
        function(F&& f) noexcept(dh::is_nothrow_constructible_v<dh::decay_t<F>, F&&>) {
            if constexpr (dh::is_pointer_v<dh::decay_t<F>>) {
                if (f == nullptr) return;
            }
            m_body.template construct<dh::decay_t<F>>(dh::forward<F>(f));
        }

        function(const function& other) { m_body.clone(other.m_body); }

        function(function&& other) noexcept { m_body.adopt(other.m_body); }

        function& operator=(const function& other) {
            if (this != &other) {
                function copy(other);
                swap(copy);
            }
            return *this;
        }

        function& operator=(function&& other) noexcept {
            if (this != &other) {
                m_body.clear();
                m_body.adopt(other.m_body);
            }
            return *this;
        }

        function& operator=(decltype(nullptr)) noexcept {
            m_body.clear();
            return *this;
        }

        template <typename F,
            typename = dh::enable_if_t<!dh::is_same_v<dh::remove_cvref_t<F>, function> && dh::is_invocable_r_v<R, dh::remove_reference_t<F>&, Args...> &&
                dh::is_copy_constructible_v<dh::decay_t<F>>>>
        function& operator=(F&& f) {
            function tmp(dh::forward<F>(f));
            swap(tmp);
            return *this;
        }

        ~function() = default;

        [[nodiscard]] explicit operator bool() const noexcept { return m_body.holds_callable(); }

        [[nodiscard]] R operator()(Args... args) const { return m_body.invoke(dh::forward<Args>(args)...); }

        void swap(function& other) noexcept {
            function tmp(static_cast<function&&>(*this));
            *this = static_cast<function&&>(other);
            other = static_cast<function&&>(tmp);
        }

        friend void swap(function& a, function& b) noexcept { a.swap(b); }

    private:
        detail::function_body<R, Args...> m_body{};
    };

}
