/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Scroll-mode input processor.
 *
 * When g_is_scrolling (or g_is_fixed_scroll) is active, converts
 * trackball REL_X / REL_Y events into REL_HWHEEL / REL_WHEEL scroll events.
 * Uses an internal accumulator so that sub-tick movements aren't lost.
 *
 * Place this BEFORE mouse_accel in the processor chain. In scroll mode
 * the output code becomes WHEEL/HWHEEL and mouse_accel (which only touches
 * REL_X/REL_Y) will pass it through unchanged.
 */

#define DT_DRV_COMPAT zmk_input_processor_scroll_mode

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>

#include "../typing_mode.h"

LOG_MODULE_REGISTER(scroll_mode, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/* Pixels of trackball motion per scroll tick. Same divisor for both
 * axes so cursor speed translates to equal scroll speed horizontally
 * and vertically. */
#define SCROLL_DIVISOR_Y 50
#define SCROLL_DIVISOR_X 50

/* Keyball-style axis snap: avoid diagonal scroll by locking to the
 * dominant axis based on a 5-sample rolling history. Ported from the
 * QMK mymap `update_snap` function. */
#define SNAP_BUF_LEN 5

typedef enum {
    SNAP_NONE,
    SNAP_X,
    SNAP_Y,
} snap_state_t;

static int32_t x_accum;
static int32_t y_accum;
static snap_state_t snap_state;
static uint8_t snap_history;
static int32_t last_abs_x_in_poll;

static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

static snap_state_t update_snap(snap_state_t current, uint8_t history) {
    uint8_t x_cnt = 0;
    for (int i = 0; i < SNAP_BUF_LEN; i++) {
        if (history & (1 << i)) x_cnt++;
    }
    if (current == SNAP_X && x_cnt <= SNAP_BUF_LEN / 2 && (history & 0x03) == 0) {
        return SNAP_Y;
    }
    if (current == SNAP_Y && x_cnt > SNAP_BUF_LEN / 2 && (history & 0x03) == 3) {
        return SNAP_X;
    }
    if (current == SNAP_NONE) {
        return (x_cnt <= SNAP_BUF_LEN / 2) ? SNAP_Y : SNAP_X;
    }
    return current;
}

static int scroll_mode_handle_event(const struct device *dev, struct input_event *event,
                                    uint32_t param1, uint32_t param2,
                                    struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (!g_is_scrolling && !g_is_fixed_scroll) {
        x_accum = 0;
        y_accum = 0;
        snap_state = SNAP_NONE;
        snap_history = 0;
        last_abs_x_in_poll = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* pmw3610 emits a pair per poll: REL_X (sync=false) + REL_Y
     * (sync=true). The sync=true on REL_Y is what triggers
     * input_listener to flush the mouse report — returning STOP on
     * either event drops that barrier and strands any pending wheel
     * bytes. So always CONTINUE; zero the event value when we're
     * suppressing output. */

    if (event->code == INPUT_REL_X) {
        /* Remember for snap update that will run on the paired REL_Y. */
        last_abs_x_in_poll = iabs(event->value);

        x_accum += event->value;
        int32_t ticks = x_accum / SCROLL_DIVISOR_X;
        if (ticks != 0 && snap_state != SNAP_Y) {
            x_accum -= ticks * SCROLL_DIVISOR_X;
            event->code = INPUT_REL_HWHEEL;
            event->value = ticks;
        } else {
            /* Snapped to Y, or sub-tick: suppress X output, keep sync. */
            event->value = 0;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code == INPUT_REL_Y) {
        /* Close out this poll: update snap based on which axis dominated. */
        int32_t abs_y = iabs(event->value);
        snap_history = (snap_history << 1) & ((1 << SNAP_BUF_LEN) - 1);
        if (last_abs_x_in_poll > abs_y) {
            snap_history |= 1;
        }
        last_abs_x_in_poll = 0;
        snap_state = update_snap(snap_state, snap_history);

        y_accum += event->value;
        int32_t ticks = y_accum / SCROLL_DIVISOR_Y;
        if (ticks != 0 && snap_state != SNAP_X) {
            y_accum -= ticks * SCROLL_DIVISOR_Y;
            event->code = INPUT_REL_WHEEL;
            /* Natural scroll: trackball down -> scroll content down = wheel up. */
            event->value = -ticks;
        } else {
            event->value = 0;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api scroll_mode_driver_api = {
    .handle_event = scroll_mode_handle_event,
};

#define SM_INST(n)                                                                                 \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &scroll_mode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SM_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
