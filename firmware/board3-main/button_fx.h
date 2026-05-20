#ifndef BUTTON_FX_H
#define BUTTON_FX_H

#include "task.h"

#include <stdint.h>

/* Physical button index. Bit 3 encodes the side (L = 0, R = 8); the low
 * three bits are the per-side button number 0..6. The encoding leaves a
 * gap at 0x07 / 0x0F so BUTTON_COUNT is 15 — array sizing follows
 * BUTTON_COUNT directly and tolerates the one unused slot. Future L2 /
 * R2 panels would extend into bit 4 (0x10 / 0x18). */
typedef enum {
    BUTTON_L0 = 0x0 | 0x0,
    BUTTON_L1 = 0x1 | 0x0,
    BUTTON_L2 = 0x2 | 0x0,
    BUTTON_L3 = 0x3 | 0x0,
    BUTTON_L4 = 0x4 | 0x0,
    BUTTON_L5 = 0x5 | 0x0,
    BUTTON_L6 = 0x6 | 0x0,
    BUTTON_R0 = 0x0 | 0x8,
    BUTTON_R1 = 0x1 | 0x8,
    BUTTON_R2 = 0x2 | 0x8,
    BUTTON_R3 = 0x3 | 0x8,
    BUTTON_R4 = 0x4 | 0x8,
    BUTTON_R5 = 0x5 | 0x8,
    BUTTON_R6 = 0x6 | 0x8,
    BUTTON_COUNT,
} ButtonIndex;

/* Bitmask values for the 16 switching-board channels. Used both as
 * single-bit identifiers (CHANNEL_MAIN) and OR-combined as a 16-bit
 * mask of currently-energised channels (the `channels` argument to
 * button_fx_on_channel_state). */
typedef enum {
    CHANNEL_MAIN = 1U << 0,
    CHANNEL_INSTRUMENTS = 1U << 1,
    CHANNEL_AUTOPILOT = 1U << 2,
    CHANNEL_NAV_BOW = 1U << 3,
    CHANNEL_NAV_STERN = 1U << 4,
    CHANNEL_NAV_STEAMING = 1U << 5,
    CHANNEL_NAV_ANCHORING = 1U << 6,
    CHANNEL_NAV_TRICOLOR = 1U << 7,
    CHANNEL_INVERTER = 1U << 8,
    CHANNEL_WATER_PUMP = 1U << 9,
    CHANNEL_FRIDGE = 1U << 10,
    CHANNEL_DECK_LIGHTS = 1U << 11,
    CHANNEL_CABIN_LIGHTS = 1U << 12,
    CHANNEL_USB = 1U << 13,
    CHANNEL_AUX_1 = 1U << 14,
    CHANNEL_AUX_2 = 1U << 15,
} Channel;

#define CHANNEL_COUNT 16

/* Register the periodic refresh task with the scheduler and zero the
 * slot state. Main-loop / init context. */
void button_fx_init(TaskController* ctrl);

/* Drop any pending / error state on the given button and return it to
 * the steady on/off rendering. No-op on out-of-range indices. */
void button_fx_clear(ButtonIndex idx);

/* Record that the user just pressed `idx` and expects the masked
 * channel bits to converge to `value`. The slot enters FX_PENDING until
 * either the next button_fx_on_channel_state confirms
 * `(channels & mask) == (value & mask)`, or the pending deadline
 * expires (FX_ERROR). Calling with the same mask+value as last time
 * just clears any stale pending/error — it does not restart the
 * deadline. */
void button_fx_set(ButtonIndex idx, uint16_t value, uint16_t mask);

/* Drive convergence of FX_PENDING / FX_ERROR slots from the switching
 * board's channel-state snapshot. `channels` is the full 16-bit mask of
 * currently-energised channels. Any slot whose `(channels & mask)`
 * matches its stored channel_value is moved back to FX_IDLE. */
void button_fx_on_channel_state(Channel channels);

#endif /* BUTTON_FX_H */
