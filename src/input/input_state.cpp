// Copyright (c) 2024 dylanhook. All rights reserved.
// Licensed under the DHGUI Proprietary License. See LICENSE in the project root.

#include "pch.h"

#include "input_state.h"

#include "core/dh/scoped_lock.h"
#include "core/dh/memory.h"
#include "core/dh/utility.h"

#include <Windows.h>

namespace dh::input {

    namespace {

        constexpr dh::u32 k_first_printable_codepoint = 0x20;
        constexpr dh::u32 k_delete_codepoint = 0x7F;

        constinit dh::atomic<dh::u64> g_input_sequence{0};

    }

    dh::u64 next_input_sequence() noexcept {
        return g_input_sequence.fetch_add(1, dh::memory_order_relaxed) + 1;
    }

    dh::optional<dh::size_t> input_state::mouse_index(unsigned int vk) noexcept {
        for (dh::size_t index = 0; index < k_mouse_button_count; ++index) {
            if (static_cast<unsigned int>(k_mouse_buttons[index]) == vk) return index;
        }
        return dh::nullopt;
    }

    void input_state::set_bit(key_bitset& bits, unsigned int vk) noexcept {
        if (vk >= k_vk_space) return;
        bits[vk / k_key_bits_per_word] |= (dh::u64{1} << (vk % k_key_bits_per_word));
    }

    void input_state::clear_bit(key_bitset& bits, unsigned int vk) noexcept {
        if (vk >= k_vk_space) return;
        bits[vk / k_key_bits_per_word] &= ~(dh::u64{1} << (vk % k_key_bits_per_word));
    }

    bool input_state::test_bit(const key_bitset& bits, unsigned int vk) noexcept {
        if (vk >= k_vk_space) return false;
        return (bits[vk / k_key_bits_per_word] & (dh::u64{1} << (vk % k_key_bits_per_word))) != 0;
    }

    void input_state::publish_bits(atomic_key_bitset& dst, const key_bitset& src) noexcept {
        for (dh::size_t i = 0; i < k_key_word_count; ++i) {
            dst[i].store(src[i], dh::memory_order_release);
        }
    }

    bool input_state::test_bit(const atomic_key_bitset& bits, unsigned int vk) noexcept {
        if (vk >= k_vk_space) return false;
        const dh::u64 word = bits[vk / k_key_bits_per_word].load(dh::memory_order_acquire);
        return (word & (dh::u64{1} << (vk % k_key_bits_per_word))) != 0;
    }

    void input_state::on_key_event(unsigned int vk, bool down) noexcept {
        const dh::scoped_lock lock{m_mutex};
        const auto transition = !down ? key_transition::released :
            test_bit(m_held, vk) ? key_transition::repeated : key_transition::pressed;
        record_key_transition(vk, transition);
    }

    void input_state::on_key_transition(unsigned int vk, key_transition transition) noexcept {
        const dh::scoped_lock lock{m_mutex};
        record_key_transition(vk, transition);
        if (transition != key_transition::released) set_bit(m_type_pending, vk);
    }

    void input_state::record_key_transition(unsigned int vk, key_transition transition) noexcept {
        if (vk >= k_vk_space) return;
        if (transition == key_transition::released) {
            clear_bit(m_held, vk);
            set_bit(m_release_pending, vk);
        } else {
            if (transition == key_transition::pressed) {
                m_press_pending[vk] = next_input_sequence();
                if (const auto index = mouse_index(vk); index) {
                    m_mouse_press_pending[*index] = m_press_pending[vk];
                    m_mouse_press_position_pending[*index] = {
                        static_cast<float>(m_mouse_x_pending), static_cast<float>(m_mouse_y_pending)};
                }
            }
            set_bit(m_held, vk);
        }
    }

    void input_state::on_key_typed(unsigned int vk) noexcept {
        const dh::scoped_lock lock{m_mutex};
        set_bit(m_type_pending, vk);
    }

    void input_state::on_typed_char(dh::u32 codepoint) noexcept {
        if (codepoint < k_first_printable_codepoint || codepoint == k_delete_codepoint) {
            return;
        }
        const dh::scoped_lock lock{m_mutex};
        if (m_typed_chars_pending.size() < k_max_typed_chars_per_frame) {
            m_typed_chars_pending.push_back(codepoint);
        } else m_typed_chars_overflow_pending = true;
    }

    void input_state::on_mouse_button(unsigned int vk, bool down) noexcept {
        on_key_event(vk, down);
    }

    void input_state::on_mouse_button(unsigned int vk, bool down, int client_x, int client_y) noexcept {
        const dh::scoped_lock lock{m_mutex};
        m_mouse_x_pending = client_x;
        m_mouse_y_pending = client_y;
        const auto transition = !down ? key_transition::released :
            test_bit(m_held, vk) ? key_transition::repeated : key_transition::pressed;
        record_key_transition(vk, transition);
    }

    dh::optional<math::vector2> input_state::mouse_press_position(unsigned int vk) const noexcept {
        const auto index = mouse_index(vk);
        if (!index || !key_pressed(vk)) return dh::nullopt;
        return m_mouse_press_position_snapshot[*index];
    }

    dh::u64 input_state::mouse_press_sequence(unsigned int vk) const noexcept {
        const auto index = mouse_index(vk);
        if (!index) return 0;
        const dh::scoped_lock lock{m_mutex};
        return m_mouse_press_snapshot[*index] == m_mouse_press_pending[*index] ? m_mouse_press_snapshot[*index] : 0;
    }

    bool input_state::claim_mouse_press(unsigned int vk) noexcept {
        const auto index = mouse_index(vk);
        if (!index || !key_pressed(vk)) return false;
        const dh::scoped_lock lock{m_mutex};
        // another press may have arrived since frame_advance, dont claim that one.
        if (m_mouse_press_snapshot[*index] != m_mouse_press_pending[*index]) return false;
        if (m_mouse_claimed[*index] == m_mouse_press_snapshot[*index]) return false;
        m_mouse_claimed[*index] = m_mouse_press_snapshot[*index];
        if (m_mouse_permitted[*index] == m_mouse_claimed[*index]) m_mouse_permitted[*index] = 0;
        return true;
    }

    bool input_state::mouse_press_claimed(unsigned int vk) const noexcept {
        const auto index = mouse_index(vk);
        if (!index || (!key_down(vk) && !key_pressed(vk))) return false;
        const dh::scoped_lock lock{m_mutex};
        return m_mouse_claimed[*index] != 0 && m_mouse_claimed[*index] == m_mouse_press_snapshot[*index];
    }

    void input_state::publish_mouse_routing(mouse_passthrough_policy policy) noexcept {
        const dh::scoped_lock lock{m_mutex};
        for (dh::size_t index = 0; index < k_mouse_button_count; ++index) {
            const auto sequence = m_mouse_press_snapshot[index];
            m_mouse_permitted[index] = m_consume_mouse.load(dh::memory_order_relaxed) && policy == mouse_passthrough_policy::allow &&
                key_down(static_cast<unsigned int>(k_mouse_buttons[index])) && sequence != m_mouse_claimed[index] ? sequence : 0;
        }
    }

    void input_state::set_consume_mouse(bool on) noexcept {
        const dh::scoped_lock lock{m_mutex};
        if (m_consume_mouse.load(dh::memory_order_relaxed) == on) return;
        m_mouse_permitted.fill(0);
        m_consume_mouse.store(on, dh::memory_order_release);
    }

    bool input_state::is_consuming_mouse() const noexcept {
        return m_consume_mouse.load(dh::memory_order_acquire);
    }

    mouse_passthrough input_state::mouse_passthrough_state() const noexcept {
        const dh::scoped_lock lock{m_mutex};
        mouse_passthrough result;
        result.consuming = m_consume_mouse.load(dh::memory_order_relaxed);
        if (!result.consuming) return result;
        const auto permitted = [&](unsigned int vk) noexcept {
            const auto index = mouse_index(vk);
            return index && test_bit(m_held, vk) && m_mouse_permitted[*index] != 0 &&
                m_mouse_permitted[*index] == m_mouse_press_pending[*index];
        };
        result.left_down = permitted(VK_LBUTTON);
        result.right_down = permitted(VK_RBUTTON);
        return result;
    }

    void input_state::on_mouse_move(int client_x, int client_y) noexcept {
        const dh::scoped_lock lock{m_mutex};
        if (m_skip_next_move) {
            m_skip_next_move = false;
            return;
        }
        m_mouse_x_pending = client_x;
        m_mouse_y_pending = client_y;
    }

    void input_state::on_mouse_warp(int client_x, int client_y) noexcept {
        const dh::scoped_lock lock{m_mutex};
        m_mouse_x_pending = client_x;
        m_mouse_y_pending = client_y;
        m_skip_next_move = true;
    }

    void input_state::on_mouse_wheel(float delta) noexcept {
        const dh::scoped_lock lock{m_mutex};
        m_wheel_pending += delta;
    }

    void input_state::on_focus_lost() noexcept {
        const dh::scoped_lock lock{m_mutex};

        for (dh::size_t i = 0; i < k_key_word_count; ++i) {
            m_release_pending[i] |= m_held[i];
            m_held[i] = 0;
        }
        m_press_pending.fill(0);
        m_mouse_press_pending.fill(0);
        m_mouse_permitted.fill(0);
        m_mouse_claimed.fill(0);
        m_type_pending.fill(0);
        m_typed_chars_pending.clear();
        m_typed_chars_overflow_pending = false;
        if (m_typing_interval_pending == ~dh::u64{0}) dh::fail_fast("input typing counter overflow", FAST_FAIL_INVALID_ARG);
        ++m_typing_interval_pending;

        m_typing_interval_live.store(m_typing_interval_pending, dh::memory_order_release);
        m_wheel_pending = 0.0f;
    }

    void input_state::frame_advance() noexcept {
        key_bitset held;
        press_sequences pressed;
        key_bitset released;
        key_bitset typed;
        dh::vector<dh::u32> typed_chars;
        int mx;
        int my;
        float wheel;
        {
            const dh::scoped_lock lock{m_mutex};
            held = m_held;
            pressed = m_press_pending;
            m_mouse_press_snapshot = m_mouse_press_pending;
            m_mouse_press_position_snapshot = m_mouse_press_position_pending;
            released = m_release_pending;
            typed = m_type_pending;
            typed_chars = dh::move(m_typed_chars_pending);
            m_typed_chars_overflowed = m_typed_chars_overflow_pending;
            m_typing_interval_snapshot = m_typing_interval_pending;
            m_typed_chars_overflow_pending = false;
            mx = m_mouse_x_pending;
            my = m_mouse_y_pending;
            wheel = m_wheel_pending;

            m_press_pending.fill(0);
            m_release_pending.fill(0);
            m_type_pending.fill(0);
            m_typed_chars_pending.clear();
            m_wheel_pending = 0.0f;
        }

        if (m_have_first_snapshot) {
            m_mouse_x_prev_snapshot = m_mouse_x_snapshot;
            m_mouse_y_prev_snapshot = m_mouse_y_snapshot;
        } else {
            m_mouse_x_prev_snapshot = mx;
            m_mouse_y_prev_snapshot = my;
            m_have_first_snapshot = true;
        }
        publish_bits(m_held_snapshot, held);
        m_pressed = pressed;
        m_released = released;
        m_typed = typed;
        m_typed_chars = dh::move(typed_chars);
        m_mouse_x_snapshot = mx;
        m_mouse_y_snapshot = my;
        m_wheel_snapshot = wheel;
    }

    bool input_state::key_down(unsigned int vk) const noexcept {
        return test_bit(m_held_snapshot, vk);
    }

    bool input_state::key_pressed(unsigned int vk) const noexcept {
        return key_press_sequence(vk) != 0;
    }

    dh::u64 input_state::key_press_sequence(unsigned int vk) const noexcept {
        return vk < k_vk_space ? m_pressed[vk] : 0;
    }

    bool input_state::key_released(unsigned int vk) const noexcept {
        return test_bit(m_released, vk);
    }

    bool input_state::key_typed(unsigned int vk) const noexcept {
        return test_bit(m_typed, vk);
    }

    bool input_state::mouse_pressed() const noexcept {
        for (const int vk : k_mouse_buttons) {
            if (key_pressed(static_cast<unsigned int>(vk))) return true;
        }
        return false;
    }

    dh::span<const dh::u32> input_state::typed_chars() const noexcept {
        return {m_typed_chars.data(), m_typed_chars.size()};
    }

    dh::vector<dh::u32> input_state::drain_typed_chars() const noexcept {
        return dh::move(m_typed_chars);
    }

    math::vector2 input_state::mouse_pos() const noexcept {
        return {static_cast<float>(m_mouse_x_snapshot), static_cast<float>(m_mouse_y_snapshot)};
    }

    math::vector2 input_state::mouse_delta() const noexcept {
        return {static_cast<float>(m_mouse_x_snapshot - m_mouse_x_prev_snapshot), static_cast<float>(m_mouse_y_snapshot - m_mouse_y_prev_snapshot)};
    }

    float input_state::mouse_wheel_delta() const noexcept {
        return m_wheel_snapshot;
    }

}
