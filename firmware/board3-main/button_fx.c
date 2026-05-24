#include "button_fx.h"

#include "controller.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"
#include "config_mode.h"

#include <xc.h>

/* Per-button RGB feedback overlay on top of the button-board LEDs.
 *
 * Each button slot runs a tiny state machine:
 *   FX_IDLE    — slot reflects the channel's steady on/off colour. The
 *                renderer checks the target value for the current intent.
 *   FX_PENDING — user just pressed and we're waiting for the channel
 *                state to reach the expected value. Renders as pulsating
 *                white. Cleared either by the next matching
 *                channel_state update or by the deadline expiring.
 *   FX_ERROR   — pending deadline expired without confirmation. Renders
 *                as flashing red until button_fx_clear() or a matching
 *                channel update arrives.
 *
 * refresh_task ticks at TICK_MS, ages the pending counters, and only
 * fires an I²C button_effect command when the rendered effect differs
 * from the last one successfully written — the button board animates
 * pulsate / flash locally, so we don't retransmit every frame.
 *
 * Scope of all public entry points: main-loop context. button_fx_set /
 * button_fx_clear are called from the controller's button dispatch,
 * button_fx_on_channel_state from controller_on_channel_changed. The
 * two I²C completions also fire from main-loop context (i2c_poll
 * dispatches them), so no INTERRUPT guards are needed inside this file. */

#define TICK_MS 10u
#define TIMEOUT_MS 1000u
#define BUTTONS_PER_GROUP 7u
#define GROUP_BIT_SHIFT 3u /* ButtonIndex encodes its side in bit 3 (L=0, R=8). */

typedef enum {
    FX_IDLE = 0,
    FX_PENDING,
    FX_ERROR,
} FxState;

typedef struct {
    FxState state;          /* FxState */
    uint8_t deadline_ticks; /* counts down in FX_PENDING; FX_ERROR on 0 */
    /* PENDING closes when (observed_channels & channel_mask) ==
     * channel_value. `channel_value` is pre-masked at button_fx_set time
     * so the comparison in on_channel_state is a single AND-and-equals. */
    uint16_t channel_mask;
    uint16_t channel_value;
} Slot;

static void refresh_task(TaskId id, void* ctx);
static uint8_t effects_differ(const CommButtonEffect* a, const CommButtonEffect* b);
static void build_button_effect(uint8_t button_group, CommButtonEffect* out);

static Slot g_slots[BUTTON_COUNT];
/* Last effect successfully transmitted to each side. Compared against the
 * freshly-built effect each tick so we skip the I²C write when nothing
 * changed. */
static CommButtonEffect g_effect_l;
static CommButtonEffect g_effect_r;
/* Single in-flight write per side; refresh_task skips a side while its
 * previous write is still on the bus. */
static uint8_t g_inflight_l;
static uint8_t g_inflight_r;

void button_fx_init(TaskController* ctrl) {
    g_inflight_l = 0;
    g_inflight_r = 0;
    comm_button_effect_init(&g_effect_l);
    comm_button_effect_init(&g_effect_r);
    for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
        g_slots[b].state = FX_IDLE;
        g_slots[b].deadline_ticks = 0;
        g_slots[b].channel_mask = 0;
        g_slots[b].channel_value = 0;
    }
    task_controller_add(ctrl, TASK_BUTTON_FX, TICK_MS, refresh_task, 0);
}

void button_fx_clear(ButtonIndex idx) {
    if (idx >= BUTTON_COUNT) {
        return;
    }
    Slot* slot = &g_slots[idx];
    slot->state = FX_IDLE;
    slot->deadline_ticks = 0;
    slot->channel_mask = 0;
    slot->channel_value = 0;
}

void button_fx_set(ButtonIndex idx, uint16_t value, uint16_t mask) {
    if (idx >= BUTTON_COUNT) {
        return;
    }
    /* Discard any value bits the caller doesn't care about so the
     * comparison in on_channel_state can use the raw `value` directly
     * instead of re-masking. */
    uint16_t masked_value = (uint16_t)(value & mask);
    Slot* slot = &g_slots[idx];
    if (slot->channel_mask != mask || slot->channel_value != masked_value) {
        slot->channel_mask = mask;
        slot->channel_value = masked_value;
        if (masked_value == 0) {
            // Switching off instantly transitions
            slot->state = FX_IDLE;
        }
        else {
            slot->state = FX_PENDING;
            slot->deadline_ticks = (uint8_t)(TIMEOUT_MS / TICK_MS);
        }
    }
}

void button_fx_on_channel_state(Channel channels) {
    for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
        Slot* s = &g_slots[b];
        uint16_t masked = (uint16_t)((uint16_t)channels & s->channel_mask);
        uint8_t state_matches = masked == s->channel_value;
        switch (s->state) {
        case FX_IDLE:
            if (!state_matches) {
                s->state = FX_ERROR;
            }
            break;
        case FX_ERROR:
            if (state_matches) {
                s->state = FX_IDLE;
            }
            break;
        case FX_PENDING:
            if (state_matches) {
                s->state = FX_IDLE;
            }
            break;
        }
    }
}

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    /* Age pending slots toward FX_ERROR. One decrement per tick, so the
     * effective deadline is (TIMEOUT_MS / TICK_MS) * TICK_MS = TIMEOUT_MS. */
    for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
        Slot* s = &g_slots[b];
        if (s->state == FX_PENDING) {
            if (s->deadline_ticks == 0) {
                s->state = FX_ERROR;
            } else {
                s->deadline_ticks--;
            }
        }
    }

    /* Per-side build + diff + send. Each side is independent — a stuck
     * in-flight on L doesn't delay R. */
    if (!g_inflight_l) {
        CommButtonEffect effect_l;
        build_button_effect(0, &effect_l);
        if (effects_differ(&effect_l, &g_effect_l)) {
            if (comm_send_button_effect(COMM_ADDRESS_BUTTON_BOARD_L, &effect_l) == I2C_RESULT_OK) {
                g_inflight_l = 1;
            }
        }
    }
    if (!g_inflight_r) {
        CommButtonEffect effect_r;
        build_button_effect(1, &effect_r);
        if (effects_differ(&effect_r, &g_effect_r)) {
            if (comm_send_button_effect(COMM_ADDRESS_BUTTON_BOARD_R, &effect_r) == I2C_RESULT_OK) {
                g_inflight_r = 1;
            }
        }
    }
}

void comm_on_button_effect_completion(I2cResult result, uint8_t addr, CommButtonEffect* effect) {
    if (addr == COMM_ADDRESS_BUTTON_BOARD_L) {
        if (result == I2C_RESULT_OK && effect) {
            g_effect_l = *effect;
        }
        g_inflight_l = 0;
    } else if (addr == COMM_ADDRESS_BUTTON_BOARD_R) {
        if (result == I2C_RESULT_OK && effect) {
            g_effect_r = *effect;
        }
        g_inflight_r = 0;
    }
}

static uint8_t effects_differ(const CommButtonEffect* a, const CommButtonEffect* b) {
    return (uint8_t)(a->outputs_76 != b->outputs_76 || a->outputs_54 != b->outputs_54 ||
                     a->outputs_32 != b->outputs_32 || a->outputs_10 != b->outputs_10);
}

/* Build the effect vector for one side. `button_group` is 0 for the L
 * panel and 1 for the R panel; ButtonIndex encodes the side in bit 3
 * (L = 0, R = 8) so `base | i` walks the seven physical buttons on that
 * side. Anything beyond 0/1 returns an init-only effect; the L2/R2
 * expansion in controller.c will need to widen this when those panels
 * land. */
static void build_button_effect(uint8_t button_group, CommButtonEffect* out) {
    if (button_group > 1) {
        return;
    }
    comm_button_effect_init(out);

    if (config_mode_active()) {
        if (button_group == 0) {
            CommButtonOutputEffect fx;
            fx.mode = COMM_EFFECT_MODE_ENABLED;
            fx.color = COMM_EFFECT_COLOR_WHITE;
            comm_button_effect_set(out, 1, fx);
            fx.mode = COMM_EFFECT_MODE_ENABLED;
            fx.color = COMM_EFFECT_COLOR_WHITE;
            comm_button_effect_set(out, 2, fx);
            fx.mode = COMM_EFFECT_MODE_ENABLED;
            fx.color = COMM_EFFECT_COLOR_GREEN;
            comm_button_effect_set(out, 3, fx);
            fx.mode = COMM_EFFECT_MODE_ENABLED;
            fx.color = COMM_EFFECT_COLOR_RED;
            comm_button_effect_set(out, 4, fx);
        }
        return;
    }

    uint8_t base = (uint8_t)(button_group << GROUP_BIT_SHIFT);
    for (uint8_t i = 0; i < BUTTONS_PER_GROUP; i++) {
        ButtonIndex button = (ButtonIndex)(base | i);
        CommButtonOutputEffect fx;
        Slot* slot = &g_slots[button];
        if (slot->state == FX_ERROR) {
            fx.mode = COMM_EFFECT_MODE_FLASHING;
            fx.color = COMM_EFFECT_COLOR_RED;
        } else if (slot->state == FX_PENDING) {
            fx.mode = COMM_EFFECT_MODE_PULSATING;
            fx.color = COMM_EFFECT_COLOR_WHITE;
        } else {
            fx.color = COMM_EFFECT_COLOR_WHITE;
            uint8_t has_value = !!(slot->channel_mask & slot->channel_value);
            fx.mode = has_value ? COMM_EFFECT_MODE_ENABLED : COMM_EFFECT_MODE_DISABLED;
        }
        (void)comm_button_effect_set(out, i, fx);
    }
}
