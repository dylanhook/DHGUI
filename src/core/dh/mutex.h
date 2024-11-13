// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh {

    class mutex final {
    public:
        constexpr mutex() noexcept : m_srw(SRWLOCK_INIT) {}

        ~mutex() = default;

        mutex(const mutex&) = delete;
        mutex(mutex&&) = delete;
        mutex& operator=(const mutex&) = delete;
        mutex& operator=(mutex&&) = delete;

        void lock() noexcept { ::AcquireSRWLockExclusive(&m_srw); }

        void unlock() noexcept { ::ReleaseSRWLockExclusive(&m_srw); }

        [[nodiscard]] bool try_lock() noexcept { return ::TryAcquireSRWLockExclusive(&m_srw) != 0; }

    private:
        friend class condition_variable;

        [[nodiscard]] PSRWLOCK native_srw() noexcept { return &m_srw; }

        SRWLOCK m_srw;
    };

    class shared_mutex final {
    public:
        constexpr shared_mutex() noexcept : m_srw(SRWLOCK_INIT) {}

        ~shared_mutex() = default;

        shared_mutex(const shared_mutex&) = delete;
        shared_mutex(shared_mutex&&) = delete;
        shared_mutex& operator=(const shared_mutex&) = delete;
        shared_mutex& operator=(shared_mutex&&) = delete;

        void lock() noexcept { ::AcquireSRWLockExclusive(&m_srw); }

        void unlock() noexcept { ::ReleaseSRWLockExclusive(&m_srw); }

        [[nodiscard]] bool try_lock() noexcept { return ::TryAcquireSRWLockExclusive(&m_srw) != 0; }

        void lock_shared() noexcept { ::AcquireSRWLockShared(&m_srw); }

        void unlock_shared() noexcept { ::ReleaseSRWLockShared(&m_srw); }

        [[nodiscard]] bool try_lock_shared() noexcept { return ::TryAcquireSRWLockShared(&m_srw) != 0; }

    private:
        SRWLOCK m_srw;
    };

    class [[nodiscard]] unique_lock final {
    public:
        explicit unique_lock(mutex& m) noexcept : m_mtx(&m), m_owns(true) { m_mtx->lock(); }

        ~unique_lock() noexcept {
            if (m_owns) m_mtx->unlock();
        }

        unique_lock(const unique_lock&) = delete;
        unique_lock& operator=(const unique_lock&) = delete;

        unique_lock(unique_lock&& other) noexcept : m_mtx(other.m_mtx), m_owns(other.m_owns) {
            other.m_mtx = nullptr;
            other.m_owns = false;
        }

        unique_lock& operator=(unique_lock&& other) noexcept {
            if (this != &other) {
                if (m_owns) m_mtx->unlock();
                m_mtx = other.m_mtx;
                m_owns = other.m_owns;
                other.m_mtx = nullptr;
                other.m_owns = false;
            }
            return *this;
        }

        [[nodiscard]] mutex* mutex_ptr() const noexcept { return m_mtx; }

    private:
        mutex* m_mtx{nullptr};
        bool m_owns{false};
    };

    class condition_variable final {
    public:
        constexpr condition_variable() noexcept : m_cv(CONDITION_VARIABLE_INIT) {}

        ~condition_variable() = default;

        condition_variable(const condition_variable&) = delete;
        condition_variable(condition_variable&&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;
        condition_variable& operator=(condition_variable&&) = delete;

        void notify_one() noexcept { ::WakeConditionVariable(&m_cv); }

        void notify_all() noexcept { ::WakeAllConditionVariable(&m_cv); }

        void wait(mutex& m) noexcept { ::SleepConditionVariableSRW(&m_cv, m.native_srw(), INFINITE, 0); }

        void wait(unique_lock& lock) noexcept { wait(*lock.mutex_ptr()); }

        template <typename Pred> void wait(unique_lock& lock, Pred pred) noexcept {
            while (!pred()) {
                wait(lock);
            }
        }

    private:
        CONDITION_VARIABLE m_cv;
    };

    template <typename M> class [[nodiscard]] shared_lock final {
    public:
        explicit shared_lock(M& m) noexcept : m_mtx(&m) { m_mtx->lock_shared(); }

        ~shared_lock() noexcept { m_mtx->unlock_shared(); }

        shared_lock(const shared_lock&) = delete;
        shared_lock(shared_lock&&) = delete;
        shared_lock& operator=(const shared_lock&) = delete;
        shared_lock& operator=(shared_lock&&) = delete;

    private:
        M* m_mtx;
    };

    template <typename M> shared_lock(M&) -> shared_lock<M>;

}
