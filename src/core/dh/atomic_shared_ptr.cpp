// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "core/dh/atomic_shared_ptr.h"

namespace dh {

    namespace {

        struct probe_type;

        [[maybe_unused]] constinit atomic_shared_ptr<probe_type*> g_constinit_probe{};

    }

}
