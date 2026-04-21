/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Click-or-key custom behavior with scroll-mode integration.
 *
 * param1: HID keycode (encoded) to emit in typing mode
 * param2: action in mouse mode:
 *   0 = left click (LCLK)
 *   1 = right click (RCLK)
 *   2 = middle click (MCLK)
 *   0xFF = scroll trigger (momentary drag-scroll; no button press)
 *
 * Keyball44 drag-scroll behavior (mouse mode):
 *   - Scroll trigger press   -> enter scroll mode (momentary)
 *   - Scroll trigger release -> exit scroll mode (unless fixed)
 *   - LCLK press in scroll   -> hold LGUI (Cmd) for Figma pinch-zoom
 *   - RCLK press in scroll   -> fix scroll mode (locks until canceled)
 *   - (Fixed scroll is canceled by any A-Z key in typing_mode listener.)
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
#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/modifiers.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define COK_SCROLL_TRIGGER 0xFF

#if IS_CENTRAL
static void press_cmd_modifier(int64_t timestamp) {
    zmk_hid_register_mods(MOD_LGUI);
    zmk_endpoints_send_report(HID_USAGE_KEY);
    raise_zmk_keycode_state_changed_from_encoded(LGUI, true, timestamp);
}

static void release_cmd_modifier(int64_t timestamp) {
    raise_zmk_keycode_state_changed_from_encoded(LGUI, false, timestamp);
    zmk_hid_unregister_mods(MOD_LGUI);
    zmk_endpoints_send_report(HID_USAGE_KEY);
}
#endif

/*
 * Mirrors the QMK mymap process_record_user flow exactly:
 *
 *   press:
 *     if BTN1 && scroll_active -> Cmd press, return
 *     if typing_mode           -> release any currently-held letter,
 *                                 register the new one, return
 *     mouse mode: record last_keycode + typing_timer for vowel autocomplete,
 *                 DRAG_SCROLL -> set scrolling, return
 *                 BTN2 in scroll -> lock fixed-scroll, return
 *                 otherwise -> normal mouse button press
 *
 *   release:
 *     if BTN1 && scroll_active -> Cmd release, return
 *     if typing_mode && held letter -> unregister letter, return
 *     if DRAG_SCROLL           -> exit scroll unless fixed, return
 *     if fixed-scroll BTN2     -> no-op
 *     otherwise -> normal mouse button release
 *
 * QMK evaluates scroll_active at release time, so releasing BTN1 after
 * DRAG_SCROLL has already been released leaves Cmd held; we accept the
 * same edge case for parity.
 */

static int on_press(struct zmk_behavior_binding *binding,
                    struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t key_encoded = binding->param1;
    uint32_t button = binding->param2;

    if (button == 0 /* LCLK */ && g_is_scrolling) {
        press_cmd_modifier(event.timestamp);
        return 0;
    }

    if (g_is_typing_mode) {
        if (g_current_pressed_key != 0 && g_current_pressed_key != key_encoded) {
            raise_zmk_keycode_state_changed_from_encoded(g_current_pressed_key, false,
                                                         event.timestamp);
        }
        g_current_pressed_key = key_encoded;
        return raise_zmk_keycode_state_changed_from_encoded(key_encoded, true, event.timestamp);
    }

    /* Mouse mode: record for vowel auto-complete. */
    g_last_keycode = key_encoded & 0xFF;
    g_typing_timer = event.timestamp;

    if (button == COK_SCROLL_TRIGGER) {
        scroll_mode_set(true);
        return 0;
    }

    if (button == 1 /* RCLK */ && g_is_scrolling) {
        scroll_mode_set_fixed(true);
        return 0;
    }

    zmk_hid_mouse_button_press(button);
    zmk_endpoints_send_mouse_report();
#endif
    return 0;
}

static int on_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t key_encoded = binding->param1;
    uint32_t button = binding->param2;

    if (button == 0 /* LCLK */ && g_is_scrolling) {
        release_cmd_modifier(event.timestamp);
        return 0;
    }

    if (g_is_typing_mode && g_current_pressed_key != 0) {
        if (g_current_pressed_key == key_encoded) {
            g_current_pressed_key = 0;
        }
        return raise_zmk_keycode_state_changed_from_encoded(key_encoded, false, event.timestamp);
    }

    if (button == COK_SCROLL_TRIGGER) {
        if (!g_is_fixed_scroll) {
            scroll_mode_set(false);
        }
        return 0;
    }

    /* RCLK press-that-locked-fixed-scroll has no release action. */
    if (g_is_fixed_scroll && button == 1) {
        return 0;
    }

    zmk_hid_mouse_button_release(button);
    zmk_endpoints_send_mouse_report();
#endif
    return 0;
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
