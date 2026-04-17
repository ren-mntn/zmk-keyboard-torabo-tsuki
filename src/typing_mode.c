/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Keyball44-style typing mode global state and listener.
 * Handles A-Z key detection to enter typing mode, and vowel
 * auto-complete for Japanese romaji input (e.g. click J then
 * type A within 300ms -> auto-insert "J" before "A").
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "typing_mode.h"

#define IS_CENTRAL (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT))

#if IS_CENTRAL
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/keys.h>
#endif

LOG_MODULE_REGISTER(typing_mode, CONFIG_ZMK_LOG_LEVEL);

/* HID usage IDs for keyboard keys (from USB HID spec) */
#define HID_A 0x04
#define HID_B 0x05
#define HID_C 0x06
#define HID_D 0x07
#define HID_E 0x08
#define HID_F 0x09
#define HID_G 0x0A
#define HID_H 0x0B
#define HID_I 0x0C
#define HID_J 0x0D
#define HID_K 0x0E
#define HID_L 0x0F
#define HID_M 0x10
#define HID_N 0x11
#define HID_O 0x12
#define HID_P 0x13
#define HID_Q 0x14
#define HID_R 0x15
#define HID_S 0x16
#define HID_T 0x17
#define HID_U 0x18
#define HID_V 0x19
#define HID_W 0x1A
#define HID_X 0x1B
#define HID_Y 0x1C
#define HID_Z 0x1D

#define USAGE_KEYBOARD HID_USAGE_KEY

/* Global state */
bool g_is_typing_mode = true;
uint32_t g_last_keycode = 0;
int64_t g_typing_timer = 0;
uint32_t g_current_pressed_key = 0;

/* Scroll mode state */
bool g_is_scrolling = false;
bool g_is_fixed_scroll = false;

void scroll_mode_set(bool active) {
    g_is_scrolling = active;
    if (!active) {
        g_is_fixed_scroll = false;
    }
}

void scroll_mode_set_fixed(bool fixed) {
    g_is_fixed_scroll = fixed;
    if (fixed) {
        g_is_scrolling = true;
    }
}

void typing_mode_set(bool enable) {
    if (g_is_typing_mode == enable) {
        return;
    }
    g_is_typing_mode = enable;

#if IS_CENTRAL
    if (!enable && g_current_pressed_key != 0) {
        /* Release any held key from the custom behavior */
        raise_zmk_keycode_state_changed_from_encoded(g_current_pressed_key, false, k_uptime_get());
    }
#endif
    if (!enable) {
        g_current_pressed_key = 0;
    }
    /* Clear pending vowel-autocomplete state on mode change */
    g_last_keycode = 0;
    g_typing_timer = 0;
}

bool typing_mode_get(void) {
    return g_is_typing_mode;
}

#if IS_CENTRAL

/* Guard against re-entry from our own injected key presses */
static bool in_vowel_inject = false;

static bool is_alpha_usage(uint16_t usage_page, uint32_t keycode) {
    return usage_page == USAGE_KEYBOARD && keycode >= HID_A && keycode <= HID_Z;
}

static bool is_vowel(uint32_t keycode) {
    return keycode == HID_A || keycode == HID_I || keycode == HID_U ||
           keycode == HID_E || keycode == HID_O;
}

static bool is_jkl_consonant(uint32_t keycode) {
    return keycode == HID_J || keycode == HID_K || keycode == HID_L;
}

static int typing_mode_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (!ev->state) {
        /* Only react to key presses, not releases */
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (in_vowel_inject) {
        /* Ignore our own re-entrant events */
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (!is_alpha_usage(ev->usage_page, ev->keycode)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Any A-Z press cancels fixed-scroll mode (Keyball44 behavior). */
    if (g_is_fixed_scroll) {
        scroll_mode_set(false);
    }

    /* Any A-Z press enters typing mode */
    if (!g_is_typing_mode) {
        g_is_typing_mode = true;
    }

    /* Vowel auto-complete: click was within TIMEOUT, now vowel -> inject consonant */
    if (g_typing_timer != 0 &&
        (ev->timestamp - g_typing_timer) <= CONFIG_ZMK_TYPING_MODE_TIMEOUT) {
        if (is_vowel(ev->keycode) && is_jkl_consonant(g_last_keycode)) {
            uint32_t consonant = g_last_keycode;
            g_typing_timer = 0;
            g_last_keycode = 0;

            /* Queue the consonant to be injected before the current vowel.
             * Using zmk_behavior_queue_add avoids listener recursion. */
            struct zmk_behavior_binding_event queue_event = {
                .layer = 0,
                .position = 0,
                .timestamp = ev->timestamp,
            };
            struct zmk_behavior_binding kp_binding = {
                .behavior_dev = "key_press",
                .param1 = consonant,
                .param2 = 0,
            };
            in_vowel_inject = true;
            zmk_behavior_queue_add(&queue_event, kp_binding, true, 0);
            zmk_behavior_queue_add(&queue_event, kp_binding, false, 5);
            in_vowel_inject = false;
        } else {
            /* Non-vowel key cancels the auto-complete window */
            g_typing_timer = 0;
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(typing_mode, typing_mode_keycode_listener);
ZMK_SUBSCRIPTION(typing_mode, zmk_keycode_state_changed);

#endif /* IS_CENTRAL */
