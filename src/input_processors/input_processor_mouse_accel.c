/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Keyball44-style adaptive mouse acceleration (per-axis).
 *
 * Parameters (from original adjust_mouse_speed):
 *   min_speed    = 0.4   -- low-speed floor for precision
 *   max_speed    = 1.8   -- high-speed cap
 *   acceleration = 0.03  -- curve steepness
 *   threshold    = 3.0   -- magnitude at which typing mode is exited
 *
 * Note: The original Keyball code uses 2D magnitude (sqrt(x^2 + y^2)),
 * but to avoid event buffering/re-emission in the Zephyr input pipeline
 * we apply the curve per-axis using |v| as the magnitude. This is a
 * close approximation and is significantly simpler.
 */

#define DT_DRV_COMPAT zmk_input_processor_mouse_accel

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include <drivers/input_processor.h>

#include "../typing_mode.h"

LOG_MODULE_REGISTER(mouse_accel, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define MA_MIN_SPEED       0.8f
#define MA_MAX_SPEED       1.0f
#define MA_ACCELERATION    0.03f
#define MA_TYPING_EXIT_THR 3.0f
#define MA_PRECISION_THR   1.5f

static int ma_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                           uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t v = event->value;
    float mag = (v < 0) ? (float)(-v) : (float)v;

    /* Exit typing mode on significant movement */
    if (mag > MA_TYPING_EXIT_THR) {
        typing_mode_set(false);
    }

    /* Adaptive speed curve */
    float mul = MA_MIN_SPEED +
                (MA_MAX_SPEED - MA_MIN_SPEED) * (1.0f - expf(-MA_ACCELERATION * mag));

    /* Preserve precision at very low speeds */
    if (mag < MA_PRECISION_THR) {
        mul = MA_MIN_SPEED + (mul - MA_MIN_SPEED) * (mag / MA_PRECISION_THR);
    }

    /* Apply multiplier, clip to int8 range */
    float scaled = (float)v * mul;
    int32_t new_v = (int32_t)scaled;
    if (new_v > 127) {
        new_v = 127;
    } else if (new_v < -127) {
        new_v = -127;
    }

    event->value = new_v;
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api ma_driver_api = {
    .handle_event = ma_handle_event,
};

#define MA_INST(n)                                                                                 \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ma_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MA_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
