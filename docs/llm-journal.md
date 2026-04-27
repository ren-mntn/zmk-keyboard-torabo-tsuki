# LLM journal: QMK → ZMK port of torabo-tsuki / Keyball-style keyboard

Audience: future LLMs debugging this repo. Not written for human readers.
Format prioritizes keyword density + symptom → root cause → fix triples
over narrative.

## Stack

- Board: BLE Micro Pro (BMP), nRF52840
- Shield: sekigon/torabo_tsuki (M variant)
- Sensor: PMW3610 trackball on SPI0, right half only
- Firmware base: zmk v0.2 + sekigon-gonnoc/zmk-pmw3610-driver@torabo-tsuki
- Central = right (trackball side), Peripheral = left
- macOS host, paired via BLE HoG

## Bootloader compatibility matrix

| bootloader | ZMK (this config) | notes |
|---|---|---|
| 1.4.0_no_msc | ❌ app doesn't start | what the repo gets flashed with by default; DFU completes but app never boots |
| 1.4.0 (MSC) | ✅ boots, BLE stable | `BLEMICROPRO` appears in Finder, UF2 drag-drop works |
| 1.3.2 (MSC) | ⚠ app boots, Mac BLE unstable | possibly older SoftDevice bundled |
| 1.3.2_no_msc | untested | |

**Use `ble_micro_pro_bootloader_1_4_0` (MSC). This is non-obvious —
sekigon's out-of-box default often ends up as the no_msc variant via
the Web Configurator, which silently produces a keyboard that accepts
DFU writes but never runs the app.**

## DFU CRC error (1.4.0_no_msc)

Symptom: Web Configurator DFU of mymap (185-257 KB) fails at the final
page with deterministic `CRC Error: Expect: 0x..., Received: 0x...`.
Small firmwares (vial, ~80 KB) flash fine.

Root cause: 1.4.0_no_msc's DFU stream chokes on firmware whose total
size isn't 4 KB aligned when the last partial page is transferred. The
sensor CRC at the final chunk never matches. Content-independent.

Fix (if forced to use that bootloader): zero-pad the raw bin to the
next 4 KB boundary BEFORE `adafruit-nrfutil pkg generate`. That util
adds 3 bytes, so pad the raw to `next_4k - 3`.

Better fix: use the MSC bootloader and drop UF2 files into
`/Volumes/BLEMICROPRO/` via `cp`. Bypasses DFU entirely.

## PMW3610 sync=true barrier (critical for horizontal scroll)

Symptom: Horizontal scroll doesn't work at all while vertical works.
Amplification doesn't help. No change with Shift+WHEEL workaround.

Root cause:
- pmw3610 driver emits a pair per poll: `REL_X (sync=false)` then
  `REL_Y (sync=true)`.
- ZMK's `input_listener` only flushes the mouse report when it sees
  `sync=true`.
- If a custom input processor returns `ZMK_INPUT_PROC_STOP` on a
  sub-tick REL_Y (zero y_accum tick), it drops the sync barrier. Any
  HWHEEL value already accumulated from the paired REL_X gets
  stranded.

Fix: never return STOP. Always return CONTINUE. When the axis didn't
cross a tick, set `event->value = 0` and fall through. Zero is a
no-op for the listener accumulator but preserves `sync=true`.

See `src/input_processors/input_processor_scroll_mode.c` — this is
the same fix pattern for both axes.

## macOS AC_PAN axis lock

Symptom: even after HWHEEL works, horizontal scroll is sluggish
compared to vertical.

Cause: macOS IOHIDFamily axis-locks when a single HID mouse report
carries both `d_scroll_x` (AC_PAN) and `d_scroll_y` (WHEEL) deltas in
succession. Whichever axis wins first suppresses the other.

Partial mitigation options:
- Enable Keyball-style axis snap in scroll_mode (ported from QMK
  mymap's `update_snap`): 5-sample rolling history, lock dominant axis
  until consistent evidence to switch. Matches QMK behavior.
- Tell the user to toggle "Natural scrolling" off (didn't confirm).
- Shift+WHEEL workaround: technically possible but unreliable here
  because our listener runs after hid_listener (see below), so
  modifier ordering isn't guaranteed. Don't bother.

## ZMK listener order (foot-gun)

**External-module `ZMK_SUBSCRIPTION(foo, zmk_keycode_state_changed)`
runs AFTER `zmk/app/src/hid_listener.c`.**

Confirmed via: link-section iteration in `event_manager.c`, no sorting.
Core listeners are linked first (compiled into `app`), module listeners
follow. `ZMK_EV_EVENT_HANDLED` only stops *subsequent* listeners —
by the time our code runs, the HID report for the original event has
already been built and queued.

Implication: from a module listener you CANNOT intercept a key before
it reaches HID. You can only:
1. React after-the-fact (e.g., flip a mode flag)
2. Raise a new event (but that re-enters the HID listener and
   triggers the "pre-release already-held key" logic added in ZMK
   v0.3-ish, causing release+press ping-pong)
3. Poke HID directly via `zmk_hid_keyboard_press/release +
   zmk_endpoints_send_report`. That works BUT again happens AFTER
   the user's original key was already sent, so ordering is wrong
   if you need "consonant before vowel"-style injection.

To intercept BEFORE HID, write a behavior. Behaviors run during keymap
resolution, before the event is even raised. A behavior's on_press can
do direct HID pokes and then call
`raise_zmk_keycode_state_changed_from_encoded` for the intended key —
that produces the correct consonant-then-vowel order.

## Vowel autocomplete ("kaki" → "あっき", "dokuji" → "doukuji")

QMK's `process_record_user` runs BEFORE the HID report is built, so
`tap_code16(last_keycode); return true;` sends consonant then vowel in
order.

First ZMK attempts (all broken, each in a different way):
1. `zmk_behavior_queue_add` for consonant — async work-queue delay
   means the consonant arrives AFTER the vowel that just passed
   through. "jaka" → "あjか" (vowel first), "kaki" → "あっき"
   (K click made vowel autocomplete fire on subsequent A, but
   consonant was queued → ended up: A, K, K, I = あっき).
2. `raise_zmk_keycode_state_changed_from_encoded` from listener —
   synchronous but listener runs after hid_listener, so the vowel is
   already sent; raising consonant afterwards produces vowel, consonant.
3. Direct HID poke from listener — same ordering problem. Plus if you
   re-raise the vowel, hid_listener's "same key held" path issues a
   release+press pair, corrupting typing ("dokuji" → "doukuji" with
   stray vowels).
4. Consume the event via `ZMK_EV_EVENT_HANDLED` + re-raise — doesn't
   help because core hid_listener has already run by the time we
   return HANDLED. All it does is block listeners after us.

Working fix: `src/behaviors/behavior_vowel_auto.c`.

- Wraps &kp for A/I/U/E/O on layer 0.
- `on_press`: if `g_typing_timer` is valid AND `g_last_keycode` is J/K/L,
  tap the consonant via direct HID (`zmk_hid_keyboard_press` +
  `zmk_endpoints_send_report` + release + send), then
  `raise_zmk_keycode_state_changed_from_encoded(vowel, true, ts)`.
- `on_release`: raise vowel release.
- Behavior phase runs BEFORE event dispatch, so HID sees:
  consonant-press, consonant-release, vowel-press. Matches QMK
  `tap_code16(consonant); return true;` exactly.

Known side effects:
- A Shift modifier active at the time carries into the tapped
  consonant (Shift+J). Same as QMK. Not currently mitigated.
- Autocomplete only fires on layer 0 AIUEO. Other layers still use
  plain `&kp`. QMK's check was layer-independent.

## Scroll mode edge cases

- Scroll trigger (K position, `&clk_or_key K 255`) pressed in mouse
  mode sets `g_is_scrolling=true`. If the user types a letter before
  releasing K, `typing_mode_keycode_listener` flips `g_is_typing_mode`
  to true. Naive release handler would take the typing-mode branch
  and never call `scroll_mode_set(false)` → scroll stuck.
- Workaround earlier: `scroll_trigger_pressed` flag set at press,
  cleared on release regardless of typing_mode.
- Final simplification: match QMK pattern exactly — on release, check
  `g_is_typing_mode && g_current_pressed_key != 0` (QMK equivalent of
  `current_pressed_key != KC_NO`). Scroll-trigger press never sets
  `g_current_pressed_key`, so the scroll-exit branch fires naturally.

Cmd held for pinch-zoom:
- LCLK (J) in scroll mode → register LGUI. Release LCLK → unregister.
- QMK evaluates `get_scroll_active()` at both press and release. If
  the scroll trigger was released between J press and J release, Cmd
  stays stuck. Same bug preserved in ZMK port for parity. Acceptable
  because natural finger order is K release last.

Fixed scroll:
- RCLK (L) in scroll mode → `scroll_mode_set_fixed(true)`.
- Any non-click keypress while fixed → cancel (broadened from QMK's
  A-Z-only to any key; user preference).

## Axis exit for typing mode

Original port had `MA_TYPING_EXIT_THR = 3.0` (magnitude threshold for
exiting typing mode on trackball motion). Light ball nudges stayed in
typing mode and J/K/L never re-clicked. Reverted to QMK's "any
non-zero delta exits typing mode" — matches
`pointing_device_task_user`'s `x != 0 || y != 0` check.

## BT profile toggle

QMK had Web Configurator to switch BT profiles. ZMK ships `&bt BT_SEL
N` / `BT_NXT` / `BT_PRV` / `BT_CLR`. For a two-device setup wanted a
single toggle: wrote `behavior_bt_toggle_01` that calls
`zmk_ble_active_profile_index()` and `zmk_ble_prof_select(target)`.
Peripheral builds don't link BLE profile functions — guard with
`IS_CENTRAL`.

## Snipe mode

PMW3610 driver already supports `snipe-layers` devicetree prop. Add a
dedicated layer (layer_7, all `&trans`), list `snipe-layers = <7>;`
in the trackball node, and activate via `&lt 7 FSLH`. Effective CPI =
`CONFIG_PMW3610_SNIPE_CPI / CONFIG_PMW3610_SNIPE_CPI_DIVIDOR`. Min
sensor CPI is 200, so use the divisor to go lower. 600/4 = 150 worked
for this user.

## BLE latency

Default ZMK connection params produce noticeable 1-3s lag on profile
switches. Adding these to both right.conf and left.conf smoothed it
(accepts higher battery drain):

```
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=12
CONFIG_BT_PERIPHERAL_PREF_LATENCY=0
CONFIG_BT_PERIPHERAL_PREF_TIMEOUT=400
```

## Flashing workflow (MSC bootloader path)

1. App running → `python3 -c "import serial, time;
   s=serial.Serial('/dev/cu.usbmodem101', 1200); time.sleep(0.1);
   s.close()"` to trigger bootloader (or short BOOT→GND if app is
   crashed).
2. `/dev/cu.usbmodem0000000000011` appears for bootloader CDC.
3. `diskutil mount /dev/diskN` if `BLEMICROPRO` doesn't auto-mount.
4. `cp firmware.uf2 /Volumes/BLEMICROPRO/` — extended-attribute copy
   errors are cosmetic, ignore.
5. App boots on a port name like `cu.usbmodem101` (1.3.2 bootloader)
   or `cu.usbmodem2101` / `cu.usbmodem1114201` (1.4.0 bootloader,
   name varies per USB port slot).

After any settings_reset firmware flash, bonds are cleared — Mac side
must remove the old `torabo-tsuki` entry under Bluetooth before
re-pairing, or connections drop instantly.

## Per-host shortcut routing (os_aware)

Same physical key, different keycode per BT profile. Behavior:
`src/behaviors/behavior_os_aware.c` (compatible
`zmk,behavior-os-aware`). param1 fires on profile 0 (Mac), param2 on
any other profile. Calls `zmk_ble_active_profile_index()` at press,
caches the chosen encoded keycode in a `pressed_kc[position]` array
indexed by `event.position` so release sends the same key even if the
profile is toggled while the key is held.

Usage:
```
&os_aware LC(PG_UP) LG(LS(TAB))   // Mac=Ctrl+PgUp, Win=Win+Shift+Tab
&os_aware LC(PG_DN) LG(TAB)       // Mac=Ctrl+PgDn, Win=Win+Tab
```

Gotcha encountered: this user runs PowerToys (or equivalent) on
Windows with **Ctrl <-> Win swapped**. Sending `LC(TAB)` from the
keyboard becomes `Win+Tab` on Windows = Task View, not next-tab. To
land as `Ctrl+Tab` on Windows we send `LG(TAB)` from the firmware so
the OS-side swap turns it into `Ctrl+Tab`. If a future user has no
swap, change Win-side params back to `LC(...)` form.

Other obvious applications of the same behavior:
- Cmd+C vs Ctrl+C clipboard combos
- Spotlight Cmd+Space vs Win key
- App-specific shortcuts where modifier conventions differ

## Where things live

- `src/typing_mode.{c,h}` — global mode flags and listener that flips
  typing_mode, cancels fixed scroll, clears autocomplete window.
- `src/behaviors/behavior_clk_or_key.c` — J/K/L clk-or-letter with
  scroll/Cmd/fixed integration. 1:1 mirror of QMK
  `process_record_user` BTN1/BTN2/DRAG_SCROLL block.
- `src/behaviors/behavior_vowel_auto.c` — AIUEO wrapper with consonant
  tap. Only way to get correct order in ZMK.
- `src/behaviors/behavior_bt_toggle_01.c` — 2-device BT profile toggle.
- `src/behaviors/behavior_os_aware.c` — per-profile keycode router.
- `src/input_processors/input_processor_mouse_accel.c` — Keyball44
  adaptive accel curve + typing-mode exit.
- `src/input_processors/input_processor_scroll_mode.c` — X/Y →
  HWHEEL/WHEEL with per-axis accumulator, sync=true preservation,
  axis snap.
