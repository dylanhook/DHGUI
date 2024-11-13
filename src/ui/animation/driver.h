// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

namespace dh::ui::animation {

    class tween;

    namespace detail {

        void retain_tween_capacity() noexcept;
        void release_tween_capacity() noexcept;

        void register_tween(tween* t) noexcept;
        void unregister_tween(tween* t) noexcept;

    }

    namespace driver {

        void tick(float dt) noexcept;

    }

}
