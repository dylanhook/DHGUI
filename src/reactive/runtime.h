// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/memory.h"
#include "core/dh/vector.h"

namespace dh::reactive::detail {

    class observer;
    class signal_base;

    enum class signal_views : dh::u8 { effective = 1u << 0, base = 1u << 1, both = effective | base };

    [[nodiscard]] constexpr bool views_overlap(signal_views read, signal_views moved) noexcept {
        return (static_cast<dh::u8>(read) & static_cast<dh::u8>(moved)) != 0;
    }

    class observer_scope final {
    public:
        explicit observer_scope(observer* obs) noexcept;
        ~observer_scope() noexcept;

        observer_scope(const observer_scope&) = delete;
        observer_scope& operator=(const observer_scope&) = delete;
        observer_scope(observer_scope&&) = delete;
        observer_scope& operator=(observer_scope&&) = delete;

    private:
        observer* m_prev;
        dh::u32 m_prev_thread;
    };

    class batch_scope final {
    public:
        batch_scope() noexcept;
        ~batch_scope() noexcept;

        batch_scope(const batch_scope&) = delete;
        batch_scope& operator=(const batch_scope&) = delete;
        batch_scope(batch_scope&&) = delete;
        batch_scope& operator=(batch_scope&&) = delete;
    };

    class observer {
    public:
        observer() noexcept = default;
        virtual ~observer();

        observer(const observer&) = delete;
        observer& operator=(const observer&) = delete;
        observer(observer&&) = delete;
        observer& operator=(observer&&) = delete;

        virtual void notify_dirty() noexcept = 0;

        virtual void run_scheduled() noexcept {}

        void add_dependency(signal_base* dep, signal_views views) noexcept;

    protected:
        void disconnect_dependencies() noexcept;

    private:
        friend class signal_base;

        struct dependency final {
        public:
            dependency(signal_base* source, signal_views read) noexcept : signal(source), views(read) {}
            ~dependency() = default;
            dependency(const dependency&) noexcept = default;
            dependency& operator=(const dependency&) noexcept = default;
            dependency(dependency&&) noexcept = default;
            dependency& operator=(dependency&&) noexcept = default;

            [[nodiscard]] bool operator==(const dependency&) const noexcept = default;

            signal_base* signal;
            signal_views views;
        };

        dh::vector<dependency> m_dependencies;
    };

    class signal_base {
    public:
        signal_base() noexcept = default;
        virtual ~signal_base();

        signal_base(const signal_base&) = delete;
        signal_base& operator=(const signal_base&) = delete;
        signal_base(signal_base&&) = delete;
        signal_base& operator=(signal_base&&) = delete;

        void track_observation(signal_views views) const noexcept;

        void notify_subscribers(signal_views moved) noexcept;

        void remove_subscriber(observer* obs, signal_views views) noexcept;

    private:
        friend class observer;

        struct subscription final {
        public:
            subscription(observer* reader, signal_views read) noexcept : obs(reader), views(read) {}
            ~subscription() = default;
            subscription(const subscription&) noexcept = default;
            subscription& operator=(const subscription&) noexcept = default;
            subscription(subscription&&) noexcept = default;
            subscription& operator=(subscription&&) noexcept = default;

            [[nodiscard]] bool operator==(const subscription&) const noexcept = default;

            observer* obs;
            signal_views views;
        };

        mutable dh::vector<subscription> m_subscribers;
    };

    void schedule_effect(observer* eff) noexcept;
    void deschedule_effect(observer* eff) noexcept;
    void flush_pending_effects_if_unbatched() noexcept;

}
