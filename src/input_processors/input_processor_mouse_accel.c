/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 *
 * Keyball44-style adaptive mouse acceleration.
 *
 * Parameters (from original adjust_mouse_speed):
 *   min_speed    = 0.4   -- low-speed floor for precision
 *   max_speed    = 1.8   -- high-speed cap
 *   acceleration = 0.03  -- curve steepness
 *   threshold    = 3.0   -- magnitude at which typing mode is exited
 *
 * Because this processor only sees one axis per event (REL_X or REL_Y),
 * we buffer the most recent x-value and use it together with the current
 * y-value to compute the magnitude on the y-event (which arrives second
 * according to the PMW3610 driver). This mirrors how Keyball's
 * keyball_on_apply_motion_to_mouse_move processes x and y together.
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

#define MA_MIN_SPEED       0.4f
#define MA_MAX_SPEED       1.8f
#define MA_ACCELERATION    0.03f
#define MA_TYPING_EXIT_THR 3.0f
#define MA_PRECISION_THR   3.0f

static int32_t pending_x = 0;

static inline int32_t clip_int8(int32_t v) {
    if (v > 127) return 127;
    if (v < -127) return -127;
    return v;
}

static int32_t apply_curve(int32_t v, float mul) {
    float scaled = (float)v * mul;
    int32_t i = (int32_t)scaled;
    return clip_int8(i);
}

static int ma_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                           uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL) {
        return 0;
    }

    if (event->code == INPUT_REL_X) {
        /* Remember x for when y arrives */
        pending_x = event->value;
        return 0;
    }

    if (event->code != INPUT_REL_Y) {
        return 0;
    }

    int32_t x = pending_x;
    int32_t y = event->value;
    pending_x = 0;

    /* Magnitude */
    float fx = (float)x;
    float fy = (float)y;
    float mag = sqrtf(fx * fx + fy * fy);

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

    /* Apply to both axes. We need to emit an x event (since we swallowed it
     * by returning 0 earlier) before letting the y event continue. */
    int32_t new_x = apply_curve(x, mul);
    int32_t new_y = apply_curve(y, mul);

    /* Mutate the y event in place, and re-emit the x event synchronously. */
    event->value = new_y;

    /* Re-inject the adjusted x value so downstream processors see it. */
    if (new_x != 0) {
        input_report_rel(event->dev, INPUT_REL_X, new_x, false, K_NO_WAIT);
    }

    return 0;
}

static struct zmk_input_processor_driver_api ma_driver_api = {
    .handle_event = ma_handle_event,
};

#define MA_INST(n)                                                                                 \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &ma_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MA_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
