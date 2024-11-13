// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "log.h"

#include "null_logger.h"

#include "core/dh/atomic.h"

namespace dh::log {

    namespace {

        constinit dh::atomic<i_logger*> g_active{nullptr};

    }

    void set_active(i_logger* logger) noexcept {
        g_active.store(logger, dh::memory_order_release);
    }

    i_logger& active() noexcept {
        if (auto* p = g_active.load(dh::memory_order_acquire); p != nullptr) {
            return *p;
        }
        return null_logger::instance();
    }

}
