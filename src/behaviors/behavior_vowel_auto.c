/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Vowel autocomplete behavior.
 *
 * param1: vowel HID usage encoded (e.g. A = ZMK_HID_USAGE(HID_USAGE_KEY, 0x04))
 *
 * Matches QMK mymap's vowel autocomplete semantics: if the previous
 * click-in-mouse-mode recorded a J/K/L consonant within
 * CONFIG_ZMK_TYPING_MODE_TIMEOUT, tap the consonant via direct HID
 * reports BEFORE raising the vowel keycode event. Because the HID poke
 * goes straight to the HID report and the vowel event then flows
 * through ZMK's normal event pipeline, the host sees: consonant press,
 * consonant release, vowel press - exactly what `tap_code16(consonant);
 * return true;` produced in QMK.
 */

#define DT_DRV_COMPAT zmk_behavior_vowel_auto

#include <zephyr/device.h>
#include <zephyr/kernel.h>
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
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_CENTRAL
static bool is_jkl(uint32_t usage_id) {
    /* HID usages: J=0x0D, K=0x0E, L=0x0F */
    return usage_id == 0x0D || usage_id == 0x0E || usage_id == 0x0F;
}
#endif

static int on_press(struct zmk_behavior_binding *binding,
                    struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    uint32_t vowel_encoded = binding->param1;

    if (g_typing_timer != 0 &&
        (event.timestamp - g_typing_timer) <= CONFIG_ZMK_TYPING_MODE_TIMEOUT &&
        is_jkl(g_last_keycode)) {
        uint32_t consonant = g_last_keycode;
        g_typing_timer = 0;
        g_last_keycode = 0;
        /* Direct HID poke - bypasses the event system so the consonant
         * ships ahead of the vowel event we raise below. */
        zmk_hid_keyboard_press(consonant);
        zmk_endpoints_send_report(HID_USAGE_KEY);
        zmk_hid_keyboard_release(consonant);
        zmk_endpoints_send_report(HID_USAGE_KEY);
    }

    return raise_zmk_keycode_state_changed_from_encoded(vowel_encoded, true,
                                                        event.timestamp);
#else
    return 0;
#endif
}

static int on_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
#if IS_CENTRAL
    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, false,
                                                        event.timestamp);
#else
    return 0;
#endif
}

static const struct behavior_driver_api behavior_vowel_auto_driver_api = {
    .binding_pressed = on_press,
    .binding_released = on_release,
};

#define VA_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_vowel_auto_driver_api);

DT_INST_FOREACH_STATUS_OKAY(VA_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
