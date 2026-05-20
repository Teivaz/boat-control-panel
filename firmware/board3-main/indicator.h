#ifndef INDICATOR_H
#define INDICATOR_H

#include "task.h"

#include <stdint.h>

/* Five-LED RGB indicator ring around the nav-lights cluster on the main
 * panel. Pull-based: the controller flips the ring on/off via
 * indicator_set_active, and the periodic refresh task queries
 * config_mode_* / controller_nav_* each frame to decide what to render:
 *
 *   config mode active           → nav-enable editor (cursor highlighted)
 *   nav config error             → all five flash red
 *   else                         → per-LED enabled / pending / errored
 *
 * Within an LED, errored beats pending beats enabled. The peak intensity
 * comes from CONFIG_ADDR_INDICATOR_BRIGHTNESS in EEPROM and is re-read
 * every refresh, so config_write_byte commits take effect within one
 * frame. */

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

/* Drive the LED ring on or off. Idempotent.
 *   on=1 : refresh task starts pulling controller_nav_* state and
 *          rendering frames to the LED chain.
 *   on=0 : refresh task early-returns; a single dark frame is shipped
 *          immediately so the WS2812 chain visibly clears (no power-save
 *          equivalent on the LED chain — only an explicit dark write
 *          extinguishes it). */
void indicator_set_active(uint8_t on);

#endif /* INDICATOR_H */
