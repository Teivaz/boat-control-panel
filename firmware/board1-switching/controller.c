#include "controller.h"

#include "adc.h"
#include "config.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "relay_mon.h"
#include "relay_out.h"
#include "sensors.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

/* ============================================================================
 * Bit-space convention
 *
 * relay_target / channel_state / the COMM_RELAY_STATE wire all share the
 * same bit ordering: wire-bit `i` is the protocol bit `i` defined in
 * board1-switching/readme.md ("Bit" column). Translation to the physical
 * shift-register pin layout happens once at the hardware boundary in
 * relay_out_write() (using its `wire_to_sr` LUT). The reverse translation
 * for monitoring lives in relay_mon_read() (using `mux*_to_wire`). The
 * main board therefore sees wire/protocol bits exclusively, matching its
 * own RELAY_* enum.
 *
 * "relay_target"   = what we commanded (last COMM_RELAY_STATE write).
 * "channel_state"  = mux-observed voltage downstream of each relay's fuse;
 *                    diverges from relay_target on a blown fuse or stuck
 *                    contactor. This is the value pushed via
 *                    COMM_CHANNEL_CHANGED and read via COMM_CHANNEL_STATE_READ.
 *
 *   Bit 0 ("main") is the master power rail. The hardware also keeps it
 *   asserted whenever any other relay is on, but the firmware sets it
 *   explicitly during the power-on transition.
 * ============================================================================
 */

/* ============================================================================
 * State shadow
 * ============================================================================
 */

static volatile uint16_t relay_target;  /* protocol bits, last write */
static volatile uint16_t channel_state; /* protocol bits, from mux   */
static volatile uint8_t level_mode_byte; /* low nibble = packed mode  */
static volatile uint8_t sensor_shadow;   /* current 3-bit sensor word */

static volatile uint16_t last_pushed_channels;
static volatile uint8_t last_pushed_sensors;
static volatile uint8_t push_dirty;

/* ============================================================================
 * Tasks
 * ============================================================================
 */

#define MONITOR_TICK_MS 50u
#define RETRY_TICK_MS TASK_MIN_MS

static void apply_target(uint16_t proto_target);
static void on_sensors_changed(uint8_t prev, uint8_t curr);
static void monitor_task(TaskId id, void* ctx);
static void retry_task(TaskId id, void* ctx);

void controller_init(TaskController* ctrl) {
    relay_target = 0;
    channel_state = 0;
    /* Restore the meter mode from EEPROM (config_init seeds it to
     * (mode_1=European, mode_0=European) = 0x05 on a virgin device). If
     * the read returns the all-ones erase pattern (0xFF) we still got
     * something sensible after masking to the 4-bit mode field — fall back
     * to the default explicitly so a corrupted byte doesn't leave both
     * channels in an unintended mode. */
    uint8_t saved = config_read_byte(CONFIG_ADDR_LEVEL_MODE);
    if (saved == 0xFF) {
        saved = ((uint8_t)COMM_METER_MODE_0_190 << 2) | (uint8_t)COMM_METER_MODE_0_190;
    }
    level_mode_byte = (uint8_t)(saved & 0x0F);
    sensor_shadow = sensors_state();
    last_pushed_channels = 0;
    last_pushed_sensors = sensor_shadow;
    push_dirty = 0;

    apply_target(0);

    sensors_set_change_handler(on_sensors_changed);

    task_controller_add(ctrl, TASK_POLL_MONITOR, MONITOR_TICK_MS, monitor_task, 0);
    task_controller_add(ctrl, TASK_COMM_RETRY, RETRY_TICK_MS, retry_task, 0);
}

/* ============================================================================
 * Inbound writes
 * ============================================================================
 */

void controller_set_relay_target(uint16_t target) {
    relay_target = target;
    apply_target(target);
}

void controller_set_level_mode(uint8_t mode_byte) {
    level_mode_byte = (uint8_t)(mode_byte & 0x0F);
}

/* ============================================================================
 * Queries
 * ============================================================================
 */

uint16_t controller_relay_target(void) {
    return relay_target;
}
uint16_t controller_channel_state(void) {
    return channel_state;
}
uint8_t controller_level_mode(void) {
    return level_mode_byte;
}
uint16_t controller_battery_mv(void) {
    return adc_read_battery_mv();
}
uint8_t controller_level(uint8_t meter_index) {
    if (meter_index == 0) {
        return adc_read_level_fresh_water();
    }
    if (meter_index == 1) {
        return adc_read_level_fuel();
    }
    return 0;
}

/* ============================================================================
 * Internal
 * ============================================================================
 */

static void apply_target(uint16_t target) {
    /* `target` is already in wire/protocol bit format — relay_out_write
     * translates to shift-register pin layout via its own wire_to_sr LUT. */
    relay_out_write(target);
}

static void on_sensors_changed(uint8_t prev, uint8_t curr) {
    (void)prev;
    INTERRUPT_PUSH;
    sensor_shadow = curr;
    push_dirty = 1;
    INTERRUPT_POP;
}

static void monitor_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    /* relay_mon_read() returns wire-bit format already (its mux*_to_wire
     * LUTs do the SR→wire mapping per the readme), so we can compare it
     * directly to the wire-bit channel_state shadow. */
    uint16_t observed = relay_mon_read();

    INTERRUPT_PUSH;
    if (observed != channel_state) {
        push_dirty = 1;
    }
    channel_state = observed;
    INTERRUPT_POP;
}

static void retry_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (!push_dirty) {
        return;
    }

    uint16_t prev_c, curr_c;
    uint8_t prev_s, curr_s;
    {
        INTERRUPT_PUSH;
        prev_c = last_pushed_channels;
        curr_c = channel_state;
        prev_s = last_pushed_sensors;
        curr_s = sensor_shadow;
        INTERRUPT_POP;
    }

    if (comm_send_channel_changed(prev_c, curr_c, prev_s, curr_s) != I2C_RESULT_OK) {
        return;
    }

    {
        INTERRUPT_PUSH;
        last_pushed_channels = curr_c;
        last_pushed_sensors = curr_s;
        if (channel_state == curr_c && sensor_shadow == curr_s) {
            push_dirty = 0;
        }
        INTERRUPT_POP;
    }
}
