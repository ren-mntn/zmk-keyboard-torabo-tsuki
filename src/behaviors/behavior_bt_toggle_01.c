/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * BT profile toggle between profile 0 and 1.
 *
 * Each press flips to whichever of profiles 0/1 is NOT active.
 * Any other selected profile is treated as "not profile 0", so the
 * press will move to profile 0.
 */

#define DT_DRV_COMPAT zmk_behavior_bt_toggle_01

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_press(struct zmk_behavior_binding *binding,
                    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

#if IS_ENABLED(CONFIG_ZMK_BLE)
    int current = zmk_ble_active_profile_index();
    uint8_t target = (current == 0) ? 1 : 0;
    zmk_ble_prof_select(target);
#endif
    return 0;
}

static int on_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return 0;
}

static const struct behavior_driver_api behavior_bt_toggle_01_driver_api = {
    .binding_pressed = on_press,
    .binding_released = on_release,
};

#define BTT_INST(n)                                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_bt_toggle_01_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BTT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
