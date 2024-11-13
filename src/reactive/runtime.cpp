// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "log/log.h"
#include "runtime.h"

#include "log/log.h"

#include "core/dh/algorithm.h"
#include "core/dh/atomic.h"
#include "core/dh/pch_contract.h"

namespace dh::reactive::detail {

    namespace {

        // only one thread can change the graph at a time.
        constinit dh::atomic<observer*> g_current_observer{nullptr};
        constinit dh::atomic<dh::u32> g_observer_thread{0};

        constinit dh::atomic<bool> g_logged_foreign_read{false};
        constinit int g_batch_depth = 0;
        constinit dh::vector<observer*> g_pending_effects{};

        constinit dh::atomic<dh::u32> g_writer_thread{0};
        constinit dh::u32 g_writer_depth{0};
        constinit dh::atomic<bool> g_logged_foreign_write{false};

        constinit dh::atomic<bool> g_logged_effect_cycle{false};

        [[nodiscard]] bool claim_graph_for_write() noexcept {
            const auto self = static_cast<dh::u32>(::GetCurrentThreadId());
            dh::u32 owner = 0;
            if (g_writer_thread.compare_exchange_strong(owner, self, dh::memory_order_acq_rel, dh::memory_order_acquire)) {
                g_writer_depth = 1;
                return true;
            }
            if (owner == self) {
                ++g_writer_depth;
                return true;
            }
            if (!g_logged_foreign_write.exchange(true, dh::memory_order_acq_rel)) {
                ::dh::log::error("reactive: update signals on the ui thread");
            }
            return false;
        }

        void release_graph_after_write() noexcept {
            if (--g_writer_depth == 0) {
                g_writer_thread.store(0, dh::memory_order_release);
            }
        }

        template <typename T> void erase_first(dh::vector<T>& vec, T value) noexcept {
            auto it = dh::find(vec.begin(), vec.end(), value);
            if (it != vec.end()) {
                vec.erase(it);
            }
        }

    }

    observer_scope::observer_scope(observer* obs) noexcept : m_prev(g_current_observer.load(dh::memory_order_relaxed)), m_prev_thread(g_observer_thread.load(dh::memory_order_relaxed)) {
        g_observer_thread.store(static_cast<dh::u32>(::GetCurrentThreadId()), dh::memory_order_relaxed);
        g_current_observer.store(obs, dh::memory_order_release);
    }

    observer_scope::~observer_scope() noexcept {
        g_current_observer.store(m_prev, dh::memory_order_release);
        g_observer_thread.store(m_prev_thread, dh::memory_order_relaxed);
    }

    batch_scope::batch_scope() noexcept {
        ++g_batch_depth;
    }

    batch_scope::~batch_scope() noexcept {
        --g_batch_depth;
        if (g_batch_depth == 0) {
            flush_pending_effects_if_unbatched();
        }
    }

    observer::~observer() {
        disconnect_dependencies();
        deschedule_effect(this);
    }

    void observer::add_dependency(signal_base* dep, signal_views views) noexcept {
        const dependency edge{dep, views};
        if (dh::find(m_dependencies.begin(), m_dependencies.end(), edge) != m_dependencies.end()) {
            return;
        }

        m_dependencies.push_back(edge);
        dep->m_subscribers.push_back(signal_base::subscription{this, views});
    }

    void observer::disconnect_dependencies() noexcept {
        for (const auto& edge : m_dependencies) {
            edge.signal->remove_subscriber(this, edge.views);
        }
        m_dependencies.clear();
    }

    signal_base::~signal_base() {
        for (const auto& reader : m_subscribers) {
            erase_first(reader.obs->m_dependencies, observer::dependency{this, reader.views});
        }
        m_subscribers.clear();
    }

    void signal_base::track_observation(signal_views views) const noexcept {
        auto* const obs = g_current_observer.load(dh::memory_order_acquire);
        if (obs == nullptr) return;

        if (g_observer_thread.load(dh::memory_order_acquire) != static_cast<dh::u32>(::GetCurrentThreadId())) {
            if (!g_logged_foreign_read.exchange(true, dh::memory_order_acq_rel)) {
                ::dh::log::error("reactive: use signal.peek() outside the ui thread");
            }
            return;
        }

        obs->add_dependency(const_cast<signal_base*>(this), views);
    }

    void signal_base::notify_subscribers(signal_views moved) noexcept {
        if (!claim_graph_for_write()) return;

        for (const auto& reader : m_subscribers) {
            if (!views_overlap(reader.views, moved)) continue;

            // queue the effect first, running it here could change the subscriber list.
            reader.obs->notify_dirty();
        }
        flush_pending_effects_if_unbatched();
        release_graph_after_write();
    }

    void signal_base::remove_subscriber(observer* obs, signal_views views) noexcept {
        erase_first(m_subscribers, subscription{obs, views});
    }

    void schedule_effect(observer* eff) noexcept {
        if (eff == nullptr) return;
        if (dh::find(g_pending_effects.begin(), g_pending_effects.end(), eff) != g_pending_effects.end()) {
            return;
        }
        g_pending_effects.push_back(eff);
    }

    void deschedule_effect(observer* eff) noexcept {
        if (eff == nullptr) return;
        erase_first(g_pending_effects, eff);
    }

    void flush_pending_effects_if_unbatched() noexcept {
        if (g_batch_depth > 0) return;
        if (!claim_graph_for_write()) return;

        constexpr dh::size_t k_cycle_ceiling = 10'000;
        dh::size_t iterations = 0;

        while (!g_pending_effects.empty()) {
            if (++iterations > k_cycle_ceiling) {
                const auto dropped = g_pending_effects.size();
                g_pending_effects.clear();
                if (!g_logged_effect_cycle.exchange(true, dh::memory_order_acq_rel)) {
                    ::dh::log::error("reactive: effects still cycling after {} runs, dropped {} pending effects", k_cycle_ceiling, dropped);
                }
                break;
            }
            observer* eff = g_pending_effects.front();
            g_pending_effects.erase(g_pending_effects.begin());
            if (eff) eff->run_scheduled();
        }
        release_graph_after_write();
    }

}
