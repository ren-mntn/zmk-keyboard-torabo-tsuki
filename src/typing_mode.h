/*
 * Copyright 2026 ren-mntn
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Typing mode state (Keyball44-style) */
extern bool g_is_typing_mode;
extern uint32_t g_last_keycode;
extern int64_t g_typing_timer;
extern uint32_t g_current_pressed_key;

void typing_mode_set(bool enable);
bool typing_mode_get(void);

/* Scroll mode state (Keyball44 drag-scroll port).
 * - g_is_scrolling: active in any scroll state (momentary OR fixed)
 * - g_is_fixed_scroll: locked state survives scroll-trigger release
 */
extern bool g_is_scrolling;
extern bool g_is_fixed_scroll;

void scroll_mode_set(bool active);
void scroll_mode_set_fixed(bool fixed);
