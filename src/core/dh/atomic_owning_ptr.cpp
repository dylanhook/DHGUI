// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "core/dh/atomic_owning_ptr.h"

namespace dh {

    namespace {

        struct probe_type;

        static_assert(sizeof(dh::atomic<probe_type*>) == sizeof(void*), "dh::atomic<T*> must match pointer size");

        static_assert(sizeof(atomic_owning_ptr<probe_type>) <= 2 * sizeof(void*), "atomic_owning_ptr must fit its pointer and alive flag");

    }

}
