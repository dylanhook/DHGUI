// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/mutex.h"
#include "core/dh/scoped_lock.h"
#include "core/dh/shared_ptr.h"

namespace dh {

    template <typename T> class atomic_shared_ptr final {
    public:
        using element_type = T;

        constexpr atomic_shared_ptr() noexcept = default;

        atomic_shared_ptr(const atomic_shared_ptr&) = delete;
        atomic_shared_ptr(atomic_shared_ptr&&) = delete;
        atomic_shared_ptr& operator=(const atomic_shared_ptr&) = delete;
        atomic_shared_ptr& operator=(atomic_shared_ptr&&) = delete;

        ~atomic_shared_ptr() = default;

        void store(dh::shared_ptr<T> new_value) noexcept {
            // destroy the old object after unlocking, its destructor can call back into this cell.
            dh::shared_ptr<T> old;
            {
                dh::scoped_lock guard{m_lock};
                old = static_cast<dh::shared_ptr<T>&&>(m_cell);
                m_cell = static_cast<dh::shared_ptr<T>&&>(new_value);
            }
        }

        [[nodiscard]] dh::shared_ptr<T> load() const noexcept {
            dh::shared_lock guard{m_lock};
            return m_cell;
        }

        void reset() noexcept { store(dh::shared_ptr<T>{}); }

    private:
        dh::shared_ptr<T> m_cell{};

        mutable shared_mutex m_lock{};
    };

}
