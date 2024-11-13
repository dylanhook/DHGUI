// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "render/image_preparation.h"
#include "core/dh/array.h"
#include "core/dh/atomic.h"
#include "core/dh/mutex.h"
#include "core/dh/scoped_lock.h"
#include "core/dh/numeric_limits.h"

namespace dh::render::image {
    namespace {
        enum class phase : dh::u8 { empty, staged, queued, processing, complete };

        struct job final {
        public:
            job() noexcept = default;
            ~job() = default;
            job(const job&) = delete;
            job& operator=(const job&) = delete;
            job(job&&) noexcept = default;
            job& operator=(job&&) noexcept = default;
            const void* owner{};
            dh::u64 serial{};
            phase state{phase::empty};
            bool canceled{false};
            dh::string input{};
            dh::size_t input_bytes{};
            format source_format{format::png};
            decoded_image output{};
            dh::optional<preparation_failure> failure{};
            dh::unique_ptr<external_image_source> external{};
            preparation_thread thread{preparation_thread::main};
            dh::u64 interval{}, timeout{}, due{}, deadline{};
        };

        [[nodiscard]] bool valid_pixels(const decoded_image& value) noexcept {
            return value.width > 0 && value.height > 0 && value.width <= k_max_dimension && value.height <= k_max_dimension &&
                value.rgba.size() == static_cast<dh::size_t>(value.width) * value.height * k_bytes_per_pixel;
        }
    }

    class image_preparation::state final {
    public:
        state() noexcept = default;
        ~state() {
            m_stop.store(true, dh::memory_order_release);
            if (m_wake) ::SetEvent(m_wake);
            if (m_worker) {
                if (::WaitForSingleObject(m_worker, INFINITE) != WAIT_OBJECT_0) dh::fail_fast("image worker did not join", FAST_FAIL_INVALID_ARG);
                ::CloseHandle(m_worker);
            }
            if (m_wake) ::CloseHandle(m_wake);
        }
        state(const state&) = delete;
        state& operator=(const state&) = delete;
        state(state&&) = delete;
        state& operator=(state&&) = delete;

        [[nodiscard]] dh::expected<void, preparation_failure> start() noexcept {
            m_wake = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!m_wake) return dh::unexpected{preparation_failure{preparation_error::event_create_failed, ::GetLastError()}};
            m_worker = ::CreateThread(nullptr, 0, entry, this, 0, nullptr);
            if (!m_worker) return dh::unexpected{preparation_failure{preparation_error::worker_create_failed, ::GetLastError()}};
            return {};
        }

        [[nodiscard]] dh::expected<preparation_id, preparation_failure> submit(const void* owner, preparation_activation activation, dh::string input, format source_format) noexcept {
            if (owner == nullptr || (activation != preparation_activation::staged && activation != preparation_activation::active) || input.empty() || input.size() > k_max_source_bytes || (source_format != format::png && source_format != format::jpeg && source_format != format::bmp))
                return dh::unexpected{preparation_failure{preparation_error::invalid_request}};
            const dh::scoped_lock lock{m_mutex};
            if (owner_jobs(owner) >= k_max_preparation_jobs_per_owner) return dh::unexpected{preparation_failure{preparation_error::owner_limit}};
            if (input.size() > k_max_preparation_input_bytes - m_input_bytes)
                return dh::unexpected{preparation_failure{preparation_error::input_limit}};
            const auto slot = free_slot();
            if (!slot) return dh::unexpected{slot.error()};
            auto& item = m_jobs[slot->slot];
            item.owner = owner;
            item.serial = slot->serial;
            item.state = activation == preparation_activation::staged ? phase::staged : phase::queued;
            item.input_bytes = input.size();
            m_input_bytes += item.input_bytes;
            item.input = dh::move(input);
            item.source_format = source_format;
            m_job_count.store(m_job_count.load(dh::memory_order_relaxed) + 1, dh::memory_order_release);
            ::SetEvent(m_wake);
            return *slot;
        }

        [[nodiscard]] dh::expected<preparation_id, preparation_failure> submit_external(const void* owner, preparation_activation activation, dh::unique_ptr<external_image_source> source, preparation_thread thread, dh::u64 interval, dh::u64 timeout) noexcept {
            const dh::u64 now = ::GetTickCount64();
            const dh::u64 room = dh::numeric_limits<dh::u64>::max() - now;
            if (!source || owner == nullptr || interval == 0 || interval > room || timeout > room || (activation != preparation_activation::staged && activation != preparation_activation::active) || (thread != preparation_thread::main && thread != preparation_thread::render && thread != preparation_thread::worker))
                return dh::unexpected{preparation_failure{preparation_error::invalid_request}};
            const dh::scoped_lock lock{m_mutex};
            if (owner_jobs(owner) >= k_max_preparation_jobs_per_owner) return dh::unexpected{preparation_failure{preparation_error::owner_limit}};
            const auto slot = free_slot();
            if (!slot) return dh::unexpected{slot.error()};
            auto& item = m_jobs[slot->slot];
            item.owner = owner;
            item.serial = slot->serial;
            item.state = activation == preparation_activation::staged ? phase::staged : phase::queued;
            item.external = dh::move(source);
            item.thread = thread;
            item.interval = interval; item.timeout = timeout;
            if (activation == preparation_activation::active) item.deadline = timeout == 0 ? 0 : now + timeout;
            m_job_count.store(m_job_count.load(dh::memory_order_relaxed) + 1, dh::memory_order_release);
            ::SetEvent(m_wake);
            return *slot;
        }

        // staged jobs wait for activate, the caller can discard them before work starts.
        void activate(const void* owner) noexcept {
            const dh::scoped_lock lock{m_mutex};
            const auto now = ::GetTickCount64();
            for (auto& item : m_jobs) {
                if (item.owner != owner || item.state != phase::staged) continue;
                item.state = phase::queued;
                if (item.external && item.timeout != 0) {
                    if (item.timeout > dh::numeric_limits<dh::u64>::max() - now) finish(item, preparation_failure{preparation_error::invalid_request});
                    else item.deadline = now + item.timeout;
                }
            }
            ::SetEvent(m_wake);
        }

        void discard_owner(const void* owner) noexcept {
            const dh::scoped_lock lock{m_mutex};
            for (auto& item : m_jobs) if (item.owner == owner && item.state != phase::empty) discard(item);
        }

        void discard(preparation_id id) noexcept {
            const dh::scoped_lock lock{m_mutex};
            if (auto* const item = find(id)) discard(*item);
        }

        [[nodiscard]] dh::expected<preparation_info, preparation_failure> inspect(preparation_id id) const noexcept {
            const dh::scoped_lock lock{m_mutex};
            const auto* const item = find(id);
            if (!item || item->canceled) return dh::unexpected{preparation_failure{preparation_error::expired}};
            preparation_info result;
            if (item->state == phase::complete) {
                if (item->failure) {
                    result.failure = item->failure;
                    result.status = item->failure->code == preparation_error::source_unavailable ? preparation_status::unavailable : preparation_status::failed;
                } else {
                    result.status = preparation_status::ready;
                    result.width = item->output.width; result.height = item->output.height;
                    result.bytes = item->output.rgba.capacity();
                }
            }
            return result;
        }

        [[nodiscard]] dh::expected<dh::optional<decoded_image>, preparation_failure> take(preparation_id id) noexcept {
            const dh::scoped_lock lock{m_mutex};
            auto* const item = find(id);
            if (!item || item->canceled) return dh::unexpected{preparation_failure{preparation_error::expired}};
            if (item->state != phase::complete) return dh::optional<decoded_image>{};
            if (item->failure) return dh::unexpected{*item->failure};
            m_output_bytes -= item->output.rgba.capacity();
            dh::optional<decoded_image> result{dh::move(item->output)};
            clear(*item);
            return result;
        }

        void pump_external(preparation_thread thread) noexcept {
            if (m_job_count.load(dh::memory_order_acquire) == 0) return;
            const dh::u64 now = ::GetTickCount64();
            for (dh::size_t index = 0; index < m_jobs.size(); ++index) {
                dh::unique_ptr<external_image_source> source;
                preparation_id id;
                {
                    const dh::scoped_lock lock{m_mutex};
                    auto& item = m_jobs[index];
                    if (item.state != phase::queued || !item.external || item.thread != thread || item.due > now) continue;
                    if (item.deadline != 0 && now >= item.deadline) {
                        item.failure = preparation_failure{preparation_error::source_timeout};
                        item.state = phase::complete;
                        item.external.reset();
                        continue;
                    }
                    source = dh::move(item.external);
                    id = {index, item.serial};
                    item.state = phase::processing;
                }
                auto sample = source->poll();
                const dh::scoped_lock lock{m_mutex};
                auto* const item = find(id);
                if (!item) dh::fail_fast("image source slot reused before drain", FAST_FAIL_INVALID_ARG);
                if (item->canceled) { clear(*item); continue; }
                if (item->deadline != 0 && ::GetTickCount64() >= item->deadline) finish(*item, preparation_failure{preparation_error::source_timeout});
                else if (!sample) finish(*item, sample.error());
                else if (sample->status == preparation_status::pending) {
                    if (item->interval > dh::numeric_limits<dh::u64>::max() - now) finish(*item, preparation_failure{preparation_error::invalid_request});
                    else {
                        item->state = phase::queued;
                        item->due = now + item->interval;
                        item->external = dh::move(source);

                        ::SetEvent(m_wake);
                    }
                } else if (sample->status == preparation_status::unavailable) finish(*item, preparation_failure{preparation_error::source_unavailable});
                else if (sample->status == preparation_status::ready) finish(*item, dh::move(sample->image));
                else finish(*item, preparation_failure{preparation_error::invalid_request});
            }
        }

    private:
        [[nodiscard]] dh::size_t owner_jobs(const void* owner) const noexcept {
            dh::size_t count = 0;
            for (const auto& item : m_jobs) if (item.state != phase::empty && item.owner == owner) ++count;
            return count;
        }
        [[nodiscard]] dh::size_t owner_output(const void* owner) const noexcept {
            dh::size_t bytes = 0;
            for (const auto& item : m_jobs) if (item.state != phase::empty && item.owner == owner) bytes += item.output.rgba.capacity();
            return bytes;
        }
        // serials keep old handles from referring to a reused slot.
        [[nodiscard]] dh::expected<preparation_id, preparation_failure> free_slot() noexcept {
            if (m_next_serial == 0) return dh::unexpected{preparation_failure{preparation_error::queue_full}};
            for (dh::size_t index = 0; index < m_jobs.size(); ++index)
                if (m_jobs[index].state == phase::empty) return preparation_id{index, m_next_serial++};
            return dh::unexpected{preparation_failure{preparation_error::queue_full}};
        }
        [[nodiscard]] job* find(preparation_id id) noexcept {
            return id.serial != 0 && id.slot < m_jobs.size() && m_jobs[id.slot].state != phase::empty && m_jobs[id.slot].serial == id.serial ? &m_jobs[id.slot] : nullptr;
        }
        [[nodiscard]] const job* find(preparation_id id) const noexcept {
            return id.serial != 0 && id.slot < m_jobs.size() && m_jobs[id.slot].state != phase::empty && m_jobs[id.slot].serial == id.serial ? &m_jobs[id.slot] : nullptr;
        }
        void clear(job& item) noexcept {
            m_input_bytes -= item.input_bytes;
            m_output_bytes -= item.output.rgba.capacity();
            item = {};
            m_job_count.store(m_job_count.load(dh::memory_order_relaxed) - 1, dh::memory_order_release);
        }
        void discard(job& item) noexcept {
            // the worker still owns this slot until it finishes.
            if (item.state == phase::processing) item.canceled = true;
            else clear(item);
        }
        void finish(job& item, preparation_failure error) noexcept {
            item.failure = error;
            item.state = phase::complete;
        }
        void finish(job& item, decoded_image image) noexcept {
            if (!valid_pixels(image)) { finish(item, preparation_failure{preparation_error::invalid_pixels}); return; }
            if (image.rgba.capacity() > k_max_preparation_output_bytes - m_output_bytes) {
                finish(item, preparation_failure{preparation_error::output_limit});
                return;
            }
            if (image.rgba.capacity() > k_max_preparation_output_bytes_per_owner - owner_output(item.owner)) {
                finish(item, preparation_failure{preparation_error::owner_output_limit});
                return;
            }
            m_output_bytes += image.rgba.capacity();
            item.output = dh::move(image);
            item.state = phase::complete;
        }
        static DWORD WINAPI entry(void* context) noexcept { static_cast<state*>(context)->run(); return 0; }
        void run() noexcept {
            while (!m_stop.load(dh::memory_order_acquire)) {
                pump_external(preparation_thread::worker);
                preparation_id id;
                dh::string input;
                format source_format{};

                dh::size_t output_allowance = 0;
                bool owner_bound = false;
                DWORD wait = INFINITE;
                {
                    const dh::scoped_lock lock{m_mutex};
                    const dh::u64 now = ::GetTickCount64();
                    for (const auto& item : m_jobs) {
                        if (item.state != phase::queued || !item.external || item.thread != preparation_thread::worker) continue;
                        constexpr dh::u64 k_maximum_wait = INFINITE - 1;
                        const auto remaining = item.due > now ? item.due - now : 0;
                        const DWORD timeout = static_cast<DWORD>(remaining < k_maximum_wait ? remaining : k_maximum_wait);
                        if (timeout < wait) wait = timeout;
                    }
                    for (auto& item : m_jobs) {
                        if (item.state != phase::queued || !item.external || item.deadline == 0) continue;
                        if (now >= item.deadline) {
                            finish(item, preparation_failure{preparation_error::source_timeout});
                            item.external.reset();
                        } else {
                            constexpr dh::u64 k_maximum_wait = INFINITE - 1;
                            const auto remaining = item.deadline - now;
                            const DWORD timeout = static_cast<DWORD>(remaining < k_maximum_wait ? remaining : k_maximum_wait);
                            if (timeout < wait) wait = timeout;
                        }
                    }
                    for (dh::size_t index = 0; index < m_jobs.size(); ++index) {
                        auto& item = m_jobs[index];
                        if (item.state != phase::queued || item.external) continue;
                        id = {index, item.serial};
                        input = dh::move(item.input);
                        source_format = item.source_format;
                        const dh::size_t pool_room = k_max_preparation_output_bytes - m_output_bytes;
                        const dh::size_t owner_room = k_max_preparation_output_bytes_per_owner - owner_output(item.owner);
                        owner_bound = owner_room < pool_room;
                        output_allowance = owner_bound ? owner_room : pool_room;
                        item.state = phase::processing;
                        break;
                    }
                }
                if (id.serial == 0) {
                    const DWORD result = ::WaitForSingleObject(m_wake, wait);
                    if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT) dh::fail_fast("image worker wait failed", FAST_FAIL_INVALID_ARG);
                    continue;
                }
                auto result = decode({reinterpret_cast<const dh::u8*>(input.data()), input.size()}, source_format, output_allowance);
                input = {};
                const dh::scoped_lock lock{m_mutex};
                auto* const item = find(id);
                if (!item) dh::fail_fast("image codec slot reused before drain", FAST_FAIL_INVALID_ARG);
                m_input_bytes -= item->input_bytes;
                item->input_bytes = 0;
                if (item->canceled) { clear(*item); continue; }
                if (result) finish(*item, dh::move(*result));
                else if (result.error().cause == decode_error::output_limit_exceeded)
                    finish(*item, preparation_failure{owner_bound ? preparation_error::owner_output_limit : preparation_error::output_limit});
                else finish(*item, preparation_failure{result.error()});
            }
        }

        mutable dh::mutex m_mutex{};
        dh::array<job, k_max_preparation_jobs> m_jobs{};

        dh::atomic<dh::size_t> m_job_count{0};
        dh::size_t m_input_bytes{}, m_output_bytes{};
        dh::u64 m_next_serial{1};
        dh::atomic<bool> m_stop{false};
        HANDLE m_wake{}, m_worker{};
    };

    dh::string to_string(preparation_error error) noexcept {
        switch (error) {
            case preparation_error::unavailable: return dh::string{"unavailable"};
            case preparation_error::allocation_failed: return dh::string{"allocation_failed"};
            case preparation_error::event_create_failed: return dh::string{"event_create_failed"};
            case preparation_error::worker_create_failed: return dh::string{"worker_create_failed"};
            case preparation_error::invalid_request: return dh::string{"invalid_request"};
            case preparation_error::queue_full: return dh::string{"queue_full"};
            case preparation_error::owner_limit: return dh::string{"owner_limit"};
            case preparation_error::input_limit: return dh::string{"input_limit"};
            case preparation_error::output_limit: return dh::string{"output_limit"};
            case preparation_error::owner_output_limit: return dh::string{"owner_output_limit"};
            case preparation_error::expired: return dh::string{"expired"};
            case preparation_error::decode_failed: return dh::string{"decode_failed"};
            case preparation_error::source_failed: return dh::string{"source_failed"};
            case preparation_error::source_unavailable: return dh::string{"source_unavailable"};
            case preparation_error::source_timeout: return dh::string{"source_timeout"};
            case preparation_error::invalid_pixels: return dh::string{"invalid_pixels"};
        }
        return dh::string{"unknown"};
    }

    image_preparation::image_preparation() noexcept = default;
    image_preparation::~image_preparation() = default;
    dh::expected<void, preparation_failure> image_preparation::initialize() noexcept {
        if (m_state) return {};
        auto initialized = dh::make_unique<state>();
        if (!initialized) return dh::unexpected{preparation_failure{preparation_error::allocation_failed}};
        const auto started = initialized->start();
        if (!started) return dh::unexpected{started.error()};
        m_state = dh::move(initialized);
        return {};
    }
    void image_preparation::shutdown() noexcept { m_state.reset(); }
    dh::expected<preparation_id, preparation_failure> image_preparation::submit(const void* owner, preparation_activation activation, dh::string bytes, format source_format) noexcept {
        if (!m_state) return dh::unexpected{preparation_failure{preparation_error::unavailable}};
        return m_state->submit(owner, activation, dh::move(bytes), source_format);
    }
    dh::expected<preparation_id, preparation_failure> image_preparation::submit_external(const void* owner, preparation_activation activation, dh::unique_ptr<external_image_source> source, preparation_thread thread, dh::u64 interval_ms, dh::u64 timeout_ms) noexcept {
        if (!m_state) return dh::unexpected{preparation_failure{preparation_error::unavailable}};
        return m_state->submit_external(owner, activation, dh::move(source), thread, interval_ms, timeout_ms);
    }
    void image_preparation::activate_owner(const void* owner) noexcept { if (m_state) m_state->activate(owner); }
    void image_preparation::discard_owner(const void* owner) noexcept { if (m_state) m_state->discard_owner(owner); }
    void image_preparation::discard(preparation_id id) noexcept { if (m_state) m_state->discard(id); }
    dh::expected<preparation_info, preparation_failure> image_preparation::inspect(preparation_id id) const noexcept {
        if (!m_state) return dh::unexpected{preparation_failure{preparation_error::unavailable}};
        return m_state->inspect(id);
    }
    dh::expected<dh::optional<decoded_image>, preparation_failure> image_preparation::take(preparation_id id) noexcept {
        if (!m_state) return dh::unexpected{preparation_failure{preparation_error::unavailable}};
        return m_state->take(id);
    }
    void image_preparation::pump_external(preparation_thread thread) noexcept {
        if (thread != preparation_thread::main && thread != preparation_thread::render)
            dh::fail_fast("pump external images on their owner thread", FAST_FAIL_INVALID_ARG);
        if (m_state) m_state->pump_external(thread);
    }
}
