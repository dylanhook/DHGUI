// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "mutex.h"

namespace dh {

    template <typename M> class scoped_lock final {
    public:
        [[nodiscard]] explicit scoped_lock(M& m) noexcept : m_mtx(&m) { m_mtx->lock(); }

        ~scoped_lock() noexcept { m_mtx->unlock(); }

        scoped_lock(const scoped_lock&) = delete;
        scoped_lock(scoped_lock&&) = delete;
        scoped_lock& operator=(const scoped_lock&) = delete;
        scoped_lock& operator=(scoped_lock&&) = delete;

    private:
        M* m_mtx;
    };

}
