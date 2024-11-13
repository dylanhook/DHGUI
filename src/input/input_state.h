// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#pragma once

#include "core/dh/memory.h"
#include "core/dh/mutex.h"
#include "core/dh/optional.h"
#include "core/dh/span.h"
#include "core/dh/vector.h"
#include "math/vector2.h"

#include "core/dh/array.h"
#include "core/dh/atomic.h"

#include <Windows.h>

namespace dh::input {

    inline constexpr unsigned k_vk_space = 256;

    enum class key_transition : dh::u8 { pressed, repeated, released };

    [[nodiscard]] dh::u64 next_input_sequence() noexcept;

    inline constexpr int k_mouse_buttons[] = {
        VK_LBUTTON,
        VK_RBUTTON,
        VK_MBUTTON,
        VK_XBUTTON1,
        VK_XBUTTON2,
    };

    inline constexpr dh::size_t k_mouse_button_count = sizeof(k_mouse_buttons) / sizeof(k_mouse_buttons[0]);
    enum class mouse_passthrough_policy : dh::u8 { allow, block };

    struct mouse_passthrough final {
    public:
        mouse_passthrough() = default;
        ~mouse_passthrough() = default;
        mouse_passthrough(const mouse_passthrough&) = default;
        mouse_passthrough& operator=(const mouse_passthrough&) = default;
        mouse_passthrough(mouse_passthrough&&) = default;
        mouse_passthrough& operator=(mouse_passthrough&&) = default;

        bool consuming{false};
        bool left_down{false};
        bool right_down{false};
    };

    class input_state final {
    public:
        input_state() noexcept = default;
        ~input_state() = default;

        input_state(const input_state&) = delete;
        input_state& operator=(const input_state&) = delete;
        input_state(input_state&&) = delete;
        input_state& operator=(input_state&&) = delete;

        void on_key_event(unsigned int vk, bool down) noexcept;

        void on_key_transition(unsigned int vk, key_transition transition) noexcept;

        void on_key_typed(unsigned int vk) noexcept;

        void on_typed_char(dh::u32 codepoint) noexcept;
        void on_mouse_button(unsigned int vk, bool down) noexcept;

        void on_mouse_button(unsigned int vk, bool down, int client_x, int client_y) noexcept;

        [[nodiscard]] dh::optional<math::vector2> mouse_press_position(unsigned int vk) const noexcept;

        [[nodiscard]] dh::u64 mouse_press_sequence(unsigned int vk) const noexcept;

        [[nodiscard]] bool claim_mouse_press(unsigned int vk) noexcept;
        [[nodiscard]] bool mouse_press_claimed(unsigned int vk) const noexcept;

        void publish_mouse_routing(mouse_passthrough_policy policy) noexcept;

        void set_consume_mouse(bool on) noexcept;
        [[nodiscard]] bool is_consuming_mouse() const noexcept;

        [[nodiscard]] mouse_passthrough mouse_passthrough_state() const noexcept;

        void on_mouse_move(int client_x, int client_y) noexcept;

        void on_mouse_warp(int client_x, int client_y) noexcept;

        void on_mouse_wheel(float delta) noexcept;

        void on_focus_lost() noexcept;

        void frame_advance() noexcept;

        [[nodiscard]] bool key_down(unsigned int vk) const noexcept;
        [[nodiscard]] bool key_pressed(unsigned int vk) const noexcept;

        [[nodiscard]] dh::u64 key_press_sequence(unsigned int vk) const noexcept;
        [[nodiscard]] bool key_released(unsigned int vk) const noexcept;

        [[nodiscard]] bool mouse_pressed() const noexcept;

        [[nodiscard]] bool key_typed(unsigned int vk) const noexcept;

        [[nodiscard]] dh::vector<dh::u32> drain_typed_chars() const noexcept;

        [[nodiscard]] dh::span<const dh::u32> typed_chars() const noexcept;

        [[nodiscard]] bool typed_chars_overflowed() const noexcept { return m_typed_chars_overflowed; }

        [[nodiscard]] dh::u64 typing_interval_revision() const noexcept { return m_typing_interval_live.load(dh::memory_order_acquire); }
        [[nodiscard]] bool typed_chars_interval_current() const noexcept { return m_typing_interval_snapshot == typing_interval_revision(); }

        static constexpr dh::size_t k_max_typed_chars_per_frame = 1024;

        [[nodiscard]] math::vector2 mouse_pos() const noexcept;
        [[nodiscard]] math::vector2 mouse_delta() const noexcept;
        [[nodiscard]] float mouse_wheel_delta() const noexcept;

    private:
        static constexpr unsigned k_key_bits_per_word = 64;
        static constexpr dh::size_t k_key_word_count = k_vk_space / k_key_bits_per_word;
        static_assert(k_key_word_count * k_key_bits_per_word == k_vk_space, "key bitset must cover every VK code");
        using key_bitset = dh::array<dh::u64, k_key_word_count>;
        using press_sequences = dh::array<dh::u64, k_vk_space>;
        using mouse_sequences = dh::array<dh::u64, k_mouse_button_count>;
        using mouse_positions = dh::array<math::vector2, k_mouse_button_count>;
        [[nodiscard]] static dh::optional<dh::size_t> mouse_index(unsigned int vk) noexcept;

        using atomic_key_bitset = dh::array<dh::atomic<dh::u64>, k_key_word_count>;

        static void publish_bits(atomic_key_bitset& dst, const key_bitset& src) noexcept;
        [[nodiscard]] static bool test_bit(const atomic_key_bitset& bits, unsigned int vk) noexcept;

        static void set_bit(key_bitset& bits, unsigned int vk) noexcept;
        static void clear_bit(key_bitset& bits, unsigned int vk) noexcept;
        [[nodiscard]] static bool test_bit(const key_bitset& bits, unsigned int vk) noexcept;

        void record_key_transition(unsigned int vk, key_transition transition) noexcept;

        mutable dh::mutex m_mutex;
        key_bitset m_held{};
        press_sequences m_press_pending{};
        key_bitset m_release_pending{};

        mouse_sequences m_mouse_press_pending{};
        mouse_positions m_mouse_press_position_pending{};

        mouse_sequences m_mouse_claimed{};
        mouse_sequences m_mouse_permitted{};
        dh::atomic<bool> m_consume_mouse{false};

        key_bitset m_type_pending{};

        dh::vector<dh::u32> m_typed_chars_pending{};
        bool m_typed_chars_overflow_pending{false};
        dh::u64 m_typing_interval_pending{1};
        dh::atomic<dh::u64> m_typing_interval_live{1};
        int m_mouse_x_pending{0};
        int m_mouse_y_pending{0};

        bool m_skip_next_move{false};
        float m_wheel_pending{0.0f};

        atomic_key_bitset m_held_snapshot{};
        press_sequences m_pressed{};
        mouse_sequences m_mouse_press_snapshot{};
        mouse_positions m_mouse_press_position_snapshot{};
        key_bitset m_released{};
        key_bitset m_typed{};

        mutable dh::vector<dh::u32> m_typed_chars{};
        bool m_typed_chars_overflowed{false};
        dh::u64 m_typing_interval_snapshot{1};
        int m_mouse_x_snapshot{0};
        int m_mouse_y_snapshot{0};
        int m_mouse_x_prev_snapshot{0};
        int m_mouse_y_prev_snapshot{0};
        float m_wheel_snapshot{0.0f};
        bool m_have_first_snapshot{false};
    };

}
