#include "button_fx.h"

#include "controller.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"

#include <xc.h>

#define TICK_MS 10u
/* Must exceed the time it takes for a channel observation to arrive: the
 * switching board's channel_changed push, or failing that the channel_state
 * poll at POLL_TICK_MS (200 ms). The previous 100 ms was shorter than the
 * poll itself, so a press that was working perfectly still timed out to
 * FX_ERROR and flashed red. */
#define TIMEOUT_MS 1000u

typedef enum {
    FX_IDLE = 0,
    FX_PENDING,
    FX_ERROR,
} FxState;

typedef struct {
    uint8_t state;
    uint8_t deadline_ticks; /* counts down in FX_PENDING */
    uint16_t expected_mask;
    uint16_t expected_value;
} Slot;

static volatile Slot slots[BUTTON_FX_SIDES][BUTTON_FX_BUTTONS_PER_SIDE];
static volatile uint16_t last_channels;

/* Previously-sent wire bytes per side: used both as the "has something
 * changed" detector and as the build buffer for the next transmit. */
static CommButtonEffect tx_effect[BUTTON_FX_SIDES];
static CommButtonEffect prev_effect[BUTTON_FX_SIDES];
/* Snapshot of the effect submitted on the in-flight transaction; on
 * success completion this becomes the new prev_effect. */
static CommButtonEffect inflight_effect[BUTTON_FX_SIDES];
static volatile uint8_t inflight[BUTTON_FX_SIDES];

static void build_side_effect(uint8_t side, CommButtonEffect* out);
static uint8_t side_address(uint8_t side);
static uint8_t effects_differ(const CommButtonEffect* a, const CommButtonEffect* b);
static void refresh_task(TaskId id, void* ctx);

void button_fx_init(TaskController* ctrl) {
    for (uint8_t s = 0; s < BUTTON_FX_SIDES; s++) {
        for (uint8_t b = 0; b < BUTTON_FX_BUTTONS_PER_SIDE; b++) {
            slots[s][b].state = FX_IDLE;
            slots[s][b].deadline_ticks = 0;
            slots[s][b].expected_mask = 0;
            slots[s][b].expected_value = 0;
        }
        comm_button_effect_init(&tx_effect[s]);
        comm_button_effect_init(&prev_effect[s]);
        comm_button_effect_init(&inflight_effect[s]);
        inflight[s] = 0;
        /* Force a first-pass transmit so the panels match our model. */
        prev_effect[s].outputs_76 = 0xFF;
    }
    last_channels = 0;
    task_controller_add(ctrl, TASK_BUTTON_FX, TICK_MS, refresh_task, 0);
}

uint8_t button_fx_is_error(uint8_t side, uint8_t idx) {
    if (side >= BUTTON_FX_SIDES || idx >= BUTTON_FX_BUTTONS_PER_SIDE) {
        return 0;
    }
    return slots[side][idx].state == FX_ERROR;
}

void button_fx_clear(uint8_t side, uint8_t idx) {
    if (side >= BUTTON_FX_SIDES || idx >= BUTTON_FX_BUTTONS_PER_SIDE) {
        return;
    }
    slots[side][idx].state = FX_IDLE;
    slots[side][idx].deadline_ticks = 0;
    slots[side][idx].expected_mask = 0;
    slots[side][idx].expected_value = 0;
}

/* Record what `side`/`idx` should converge to. Shared by the press fast-path
 * and the periodic pull in refresh_task, and idempotent: an unchanged
 * expectation leaves the slot alone, so re-asserting it every tick neither
 * restarts a pending deadline nor clears a legitimate error. */
static void track(uint8_t side, uint8_t idx, uint16_t mask, uint16_t value) {
    Slot* s = (Slot*)&slots[side][idx];
    uint16_t v = (uint16_t)(value & mask);
    if (s->expected_mask == mask && s->expected_value == v) {
        return;
    }
    s->expected_mask = mask;
    s->expected_value = v;
    /* Switching off transitions instantly, and the fast path covers the case
     * where the bus already shows the requested state — neither has anything
     * to wait for, and pending on them only created a deadline that could
     * expire into a spurious red. */
    if (v == 0 || (last_channels & mask) == v) {
        s->state = FX_IDLE;
        s->deadline_ticks = 0;
    } else {
        s->state = FX_PENDING;
        s->deadline_ticks = (uint8_t)(TIMEOUT_MS / TICK_MS);
    }
}

void button_fx_notify_press(uint8_t side, uint8_t idx, uint16_t mask, uint16_t value) {
    if (side >= BUTTON_FX_SIDES || idx >= BUTTON_FX_BUTTONS_PER_SIDE) {
        return;
    }
    track(side, idx, mask, value);
}

void button_fx_on_channel_state(uint16_t channels) {
    last_channels = channels;
    for (uint8_t side = 0; side < BUTTON_FX_SIDES; side++) {
        for (uint8_t b = 0; b < BUTTON_FX_BUTTONS_PER_SIDE; b++) {
            Slot* s = (Slot*)&slots[side][b];
            if (s->expected_mask == 0) {
                continue; /* button not mapped to a channel — always dark */
            }
            const uint8_t matches = ((channels & s->expected_mask) == s->expected_value);

            if (matches) {
                /* Both PENDING and ERROR converge back to IDLE once the bus
                 * agrees with what was asked for. Considering only PENDING
                 * left a slot that had timed out red for as long as the panel
                 * was up — even after the channel reached the requested state
                 * — until the user pressed that button again. */
                if (s->state != FX_IDLE) {
                    s->state = FX_IDLE;
                    s->deadline_ticks = 0;
                }
            } else if (s->state == FX_IDLE && s->expected_value != 0) {
                /* A channel that was on and settled has stopped reading
                 * voltage — a blown fuse or a lost supply. That is the same
                 * fault as a press that never took effect, so it renders the
                 * same way. Without this a settled slot never re-examined
                 * itself and kept showing solid white on a dead channel.
                 *
                 * Only asserted when the channel was asked to be on: the
                 * inverse (commanded off but reading voltage) is not a fault
                 * this panel can act on, and flagging it would light buttons
                 * for channels the user has deliberately switched off. */
                s->state = FX_ERROR;
                s->deadline_ticks = 0;
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Rendering / outbound
 * ---------------------------------------------------------------------------
 */

static uint8_t side_address(uint8_t side) {
    return (side == 0) ? COMM_ADDRESS_BUTTON_BOARD_L : COMM_ADDRESS_BUTTON_BOARD_R;
}

static uint8_t effects_differ(const CommButtonEffect* a, const CommButtonEffect* b) {
    return (uint8_t)(a->outputs_76 != b->outputs_76 || a->outputs_54 != b->outputs_54 ||
                     a->outputs_32 != b->outputs_32 || a->outputs_10 != b->outputs_10);
}

static void build_side_effect(uint8_t side, CommButtonEffect* out) {
    comm_button_effect_init(out);
    for (uint8_t b = 0; b < BUTTON_FX_BUTTONS_PER_SIDE; b++) {
        CommButtonOutputEffect fx;
        FxState st = (FxState)slots[side][b].state;
        if (st == FX_ERROR) {
            fx.mode = COMM_EFFECT_MODE_FLASHING;
            fx.color = COMM_EFFECT_COLOR_RED;
        } else if (st == FX_PENDING) {
            fx.mode = COMM_EFFECT_MODE_PULSATING;
            fx.color = COMM_EFFECT_COLOR_WHITE;
        } else {
            const Slot* s = (const Slot*)&slots[side][b];
            fx.color = COMM_EFFECT_COLOR_WHITE;
            fx.mode = (s->expected_mask & s->expected_value) ? COMM_EFFECT_MODE_ENABLED : COMM_EFFECT_MODE_DISABLED;
        }
        (void)comm_button_effect_set(out, b, fx);
    }
}

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    /* Refresh every button's expectation from the commanded relay target.
     * Pulling here rather than only on press is what lets an untouched button
     * show a fault — a channel that loses voltage on its own now goes red —
     * and it keeps the compound nav buttons following the lights actually
     * energised. Pulling from this task rather than pushing from
     * recompute_target keeps it off the deep inbound-message call chain. */
    const uint16_t target = controller_relay_target();
    for (uint8_t side = 0; side < BUTTON_FX_SIDES; side++) {
        for (uint8_t b = 0; b < BUTTON_FX_BUTTONS_PER_SIDE; b++) {
            track(side, b, controller_button_channel_mask(side, b), target);
        }
    }

    /* Age any pending slot towards the error state. The read-modify-write on
     * s->state must be atomic against on_channel_state, which can transition
     * PENDING -> IDLE from ISR context; without the guard we could clobber
     * that IDLE with ERROR and strand the slot. */
    for (uint8_t side = 0; side < BUTTON_FX_SIDES; side++) {
        for (uint8_t b = 0; b < BUTTON_FX_BUTTONS_PER_SIDE; b++) {
            Slot* s = (Slot*)&slots[side][b];
            INTERRUPT_PUSH;
            if (s->state == FX_PENDING) {
                if (s->deadline_ticks == 0) {
                    s->state = FX_ERROR;
                } else {
                    s->deadline_ticks--;
                }
            }
            INTERRUPT_POP;
        }
    }

    /* Push each side's effect vector if it has changed since the last
     * successfully-sent value. While a transmit for this side is in flight
     * we skip — when the completion fires it'll reflect that snapshot, and
     * the next refresh tick will resubmit only if there's still a delta.
     * The button board animates pulsating / flashing locally, so there's
     * no need to keep retransmitting for animation frames. */
    for (uint8_t side = 0; side < BUTTON_FX_SIDES; side++) {
        if (inflight[side]) {
            continue;
        }
        build_side_effect(side, &tx_effect[side]);
        if (!effects_differ(&tx_effect[side], &prev_effect[side])) {
            continue;
        }
        inflight_effect[side] = tx_effect[side];
        inflight[side] = 1;
        if (comm_send_button_effect(side_address(side), &inflight_effect[side]) != I2C_RESULT_OK) {
            inflight[side] = 0;
        }
    }
}

void button_fx_on_effect_completion(I2cResult result, uint8_t addr) {
    uint8_t side = (addr == COMM_ADDRESS_BUTTON_BOARD_L) ? 0 : 1;
    /* Latch the sent value as the new baseline only when it actually
     * landed. On failure the baseline stays put, so the next refresh tick
     * sees the same delta and resubmits. */
    if (result == I2C_RESULT_OK) {
        prev_effect[side] = inflight_effect[side];
    }
    inflight[side] = 0;
}
