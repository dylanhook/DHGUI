// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "runtime.h"

#include "core/dh/type_traits.h"
#include "core/dh/utility.h"

namespace dh::reactive {

    using batch_guard = detail::batch_scope;

    template <typename F> auto batch(F&& fn) noexcept -> dh::invoke_result_t<F> {
        const batch_guard guard;
        if constexpr (dh::is_void_v<dh::invoke_result_t<F>>) {
            dh::forward<F>(fn)();
        } else {
            return dh::forward<F>(fn)();
        }
    }

}
