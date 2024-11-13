// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh {

    template <typename T1, typename T2> struct pair final {
        using first_type = T1;
        using second_type = T2;

        T1 first;
        T2 second;
    };

    template <typename T1, typename T2> pair(T1, T2) -> pair<T1, T2>;

}
