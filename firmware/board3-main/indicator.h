#ifndef INDICATOR_H
#define INDICATOR_H

#include "task.h"

#include <stdint.h>

/* Five-LED RGB indicator ring around the nav-lights cluster on the main
 * panel. The controller drives it via a small mode machine:
 *
 *   OFF    — all five LEDs dark.
 *   ERROR  — all five flash red at ~2 Hz.
 *   CONFIG — renders the nav-enable editor: each LED reflects whether
 *            the corresponding nav light is enabled in EEPROM, with the
 *            caller-supplied cursor index highlighted in green.
 *   ON     — per-LED rendering driven by three 5-bit bitfields supplied
 *            by the caller (enabled / pending / errored). Within a
 *            single LED, errored beats pending beats enabled.
 *
 * The peak intensity used by every mode comes from
 * CONFIG_ADDR_INDICATOR_BRIGHTNESS in EEPROM — change it via
 * config_write_byte and the next refresh frame picks it up. */

/* 5-bit mask of the navigation lights in NAV_LIGHT_* bit order
 * (anchoring / tricolor / steaming / bow / stern at bits 0..4). The
 * union exposes `raw` for compact iteration / wire compatibility and
 * the named fields for unambiguous call-site initialisation. Matches
 * the bitfield-union pattern already used by CommButtonOutputEffect in
 * libcomm; XC8's LSB-first bitfield allocation puts `anchoring` at
 * bit 0, matching NAV_LIGHT_ANCHORING. */
typedef union {
    uint8_t raw;
    struct {
        uint8_t anchoring : 1;
        uint8_t tricolor : 1;
        uint8_t steaming : 1;
        uint8_t bow : 1;
        uint8_t stern : 1;
    };
} NavLights;

void indicator_init(TaskController* ctrl);

/* All five LEDs flash red until another mode is set. */
void indicator_set_error(void);

/* All five LEDs dark until another mode is set. */
void indicator_clear(void);

/* Render the nav-enable editor. `selected_idx` (0..4) highlights one
 * LED green-or-dim-red; the other four render blue (bright = enabled,
 * dim = disabled). The enabled mask is read internally from
 * config_get_nav_enabled_mask(), so the caller only passes the cursor.
 * An out-of-range index simply highlights nothing — all five render as
 * non-cursor LEDs. */
void indicator_set_config(uint8_t selected_idx);

/* Normal-mode rendering. Within an LED, errored overrides pending
 * overrides enabled; an LED set in none of the three masks renders
 * dark. */
void indicator_set_on(NavLights enabled, NavLights pending, NavLights errored);

#endif /* INDICATOR_H */
