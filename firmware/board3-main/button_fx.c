#include "button_fx.h"

#include "controller.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"

#include <xc.h>

#define TICK_MS 10u
#define TIMEOUT_MS 100u

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
static volatile uint16_t last_physical;

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
static void on_effect_done_l(uint8_t* rx, uint8_t rx_len, void* ctx);
static void on_effect_done_r(uint8_t* rx, uint8_t rx_len, void* ctx);

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
    last_physical = 0;
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

void button_fx_notify_press(uint8_t side, uint8_t idx, uint16_t mask, uint16_t value) {
    if (side >= BUTTON_FX_SIDES || idx >= BUTTON_FX_BUTTONS_PER_SIDE) {
        return;
    }
    Slot* s = (Slot*)&slots[side][idx];
    s->expected_mask = mask;
    s->expected_value = (uint16_t)(value & mask);
    /* Fast-path: if the physical bus already matches the requested state
     * there is nothing to wait for. */
    if ((last_physical & mask) == s->expected_value) {
        s->state = FX_IDLE;
        s->deadline_ticks = 0;
    } else {
        s->state = FX_PENDING;
        s->deadline_ticks = (uint8_t)(TIMEOUT_MS / TICK_MS);
    }
}

void button_fx_on_relay_physical(uint16_t physical) {
    last_physical = physical;
    for (uint8_t side = 0; side < BUTTON_FX_SIDES; side++) {
        for (uint8_t b = 0; b < BUTTON_FX_BUTTONS_PER_SIDE; b++) {
            Slot* s = (Slot*)&slots[side][b];
            if (s->state != FX_PENDING) {
                continue;
            }
            if ((physical & s->expected_mask) == s->expected_value) {
                s->state = FX_IDLE;
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
            fx.color = COMM_EFFECT_COLOR_WHITE;
            fx.mode = controller_button_base_on(side, b) ? COMM_EFFECT_MODE_ENABLED : COMM_EFFECT_MODE_DISABLED;
        }
        (void)comm_button_effect_set(out, b, fx);
    }
}

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    /* Age any pending slot towards the error state. The read-modify-write on
     * s->state must be atomic against on_relay_physical, which can transition
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
        I2cCompletion cb = (side == 0) ? on_effect_done_l : on_effect_done_r;
        inflight[side] = 1;
        if (comm_send_button_effect(side_address(side), &inflight_effect[side], cb, 0) != I2C_RESULT_OK) {
            inflight[side] = 0;
        }
    }
}

static void on_effect_done_l(uint8_t* rx, uint8_t rx_len, void* ctx) {
    (void)rx;
    (void)rx_len;
    (void)ctx;
    /* Completion fires on write success; final-retry failure is silent. */
    prev_effect[0] = inflight_effect[0];
    inflight[0] = 0;
}

static void on_effect_done_r(uint8_t* rx, uint8_t rx_len, void* ctx) {
    (void)rx;
    (void)rx_len;
    (void)ctx;
    prev_effect[1] = inflight_effect[1];
    inflight[1] = 0;
}
