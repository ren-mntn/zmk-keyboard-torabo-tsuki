/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Click-or-key custom behavior.
 * param1: HID keycode (encoded) to emit in typing mode
 * param2: mouse button bit (1 = LCLK, 2 = RCLK, 4 = MCLK) for mouse mode
 *
 * Keyball44-style: always acts as both a mouse button AND records the
 * associated key for vowel auto-complete in the typing mode listener.
 */

#define DT_DRV_COMPAT zmk_behavior_clk_or_key

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#include "../typing_mode.h"

#define IS_CENTRAL (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT))

#if IS_CENTRAL
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_press(struct zmk_behavior_binding *binding,
                    struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t key_encoded = binding->param1;
    uint32_t button_bit = binding->param2;

    if (g_is_typing_mode) {
        /* Typing mode: send the key */
        if (g_current_pressed_key != 0 && g_current_pressed_key != key_encoded) {
            raise_zmk_keycode_state_changed_from_encoded(g_current_pressed_key, false,
                                                         event.timestamp);
        }
        g_current_pressed_key = key_encoded;
        return raise_zmk_keycode_state_changed_from_encoded(key_encoded, true, event.timestamp);
    }

    /* Mouse mode: send the mouse button, record for vowel auto-complete */
    g_last_keycode = key_encoded & 0xFF; /* HID usage only, no mods */
    g_typing_timer = event.timestamp;

    zmk_hid_mouse_button_press(button_bit);
    zmk_endpoints_send_mouse_report();
#endif
    return 0; /* ZMK_BEHAVIOR_OPAQUE */
}

static int on_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t key_encoded = binding->param1;
    uint32_t button_bit = binding->param2;

    if (g_is_typing_mode) {
        if (g_current_pressed_key == key_encoded) {
            g_current_pressed_key = 0;
        }
        return raise_zmk_keycode_state_changed_from_encoded(key_encoded, false, event.timestamp);
    }

    zmk_hid_mouse_button_release(button_bit);
    zmk_endpoints_send_mouse_report();
#endif
    return 0; /* ZMK_BEHAVIOR_OPAQUE */
}

static const struct behavior_driver_api behavior_clk_or_key_driver_api = {
    .binding_pressed = on_press,
    .binding_released = on_release,
};

#define COK_INST(n)                                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_clk_or_key_driver_api);

DT_INST_FOREACH_STATUS_OKAY(COK_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
