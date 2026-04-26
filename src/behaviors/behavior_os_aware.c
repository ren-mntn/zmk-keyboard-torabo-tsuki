/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * OS-aware key press behavior.
 *
 * param1: encoded keycode to send when BT profile 0 is active (Mac)
 * param2: encoded keycode to send when any other profile is active (Win/etc)
 *
 * On press, queries `zmk_ble_active_profile_index()` and routes to the
 * matching keycode. The chosen encoded keycode is cached per keymap
 * position so the matching release sends the same key even if the user
 * switches BT profile while holding the key.
 */

#define DT_DRV_COMPAT zmk_behavior_os_aware

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#define IS_CENTRAL (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT))

#if IS_CENTRAL
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#endif
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_CENTRAL
/* Cache pressed-keycode per matrix position so release mirrors press
 * even if the active profile changed in between. ZMK_KEYMAP_LEN is
 * inaccessible here; pick a generous static cap. */
#define OSAW_MAX_POSITIONS 256
static uint32_t pressed_kc[OSAW_MAX_POSITIONS];

static uint32_t pick_keycode(uint32_t mac_kc, uint32_t win_kc) {
#if IS_ENABLED(CONFIG_ZMK_BLE)
    int profile = zmk_ble_active_profile_index();
    return (profile == 0) ? mac_kc : win_kc;
#else
    return mac_kc;
#endif
}
#endif

static int on_press(struct zmk_behavior_binding *binding,
                    struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t kc = pick_keycode(binding->param1, binding->param2);
    if (event.position < OSAW_MAX_POSITIONS) {
        pressed_kc[event.position] = kc;
    }
    return raise_zmk_keycode_state_changed_from_encoded(kc, true, event.timestamp);
#else
    return 0;
#endif
}

static int on_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t kc = 0;
    if (event.position < OSAW_MAX_POSITIONS) {
        kc = pressed_kc[event.position];
        pressed_kc[event.position] = 0;
    }
    if (kc == 0) {
        /* Fallback: re-pick now. */
        kc = pick_keycode(binding->param1, binding->param2);
    }
    return raise_zmk_keycode_state_changed_from_encoded(kc, false, event.timestamp);
#else
    return 0;
#endif
}

static const struct behavior_driver_api behavior_os_aware_driver_api = {
    .binding_pressed = on_press,
    .binding_released = on_release,
};

#define OSAW_INST(n)                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_os_aware_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OSAW_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
