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

/* Pixels of trackball motion per scroll tick. Tune as needed.
 * macOS AC_PAN (horizontal) responds slowly to small values, so X
 * accumulates quickly AND the output tick is amplified. */
#define SCROLL_DIVISOR_Y 50
#define SCROLL_DIVISOR_X 50
#define SCROLL_X_OUTPUT_MULT 5

static int32_t x_accum;
static int32_t y_accum;

static int scroll_mode_handle_event(const struct device *dev, struct input_event *event,
                                    uint32_t param1, uint32_t param2,
                                    struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (!g_is_scrolling && !g_is_fixed_scroll) {
        /* Not scrolling: reset accumulator and pass through. */
        x_accum = 0;
        y_accum = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code == INPUT_REL_X) {
        x_accum += event->value;
        int32_t ticks = x_accum / SCROLL_DIVISOR_X;
        if (ticks != 0) {
            x_accum -= ticks * SCROLL_DIVISOR_X;
            event->code = INPUT_REL_HWHEEL;
            event->value = ticks * SCROLL_X_OUTPUT_MULT;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        /* Sub-tick: drop this event. */
        return ZMK_INPUT_PROC_STOP;
    }

    if (event->code == INPUT_REL_Y) {
        y_accum += event->value;
        int32_t ticks = y_accum / SCROLL_DIVISOR_Y;
        if (ticks != 0) {
            y_accum -= ticks * SCROLL_DIVISOR_Y;
            event->code = INPUT_REL_WHEEL;
            /* Natural scroll: trackball down -> scroll content down = wheel up. */
            event->value = -ticks;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        return ZMK_INPUT_PROC_STOP;
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
