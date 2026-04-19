#include "controller.h"
#include "adc.h"
#include "i2c.h"
#include "libcomm.h"
#include "relay_mon.h"
#include "relay_out.h"
#include "sensors.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

/* ============================================================================
 * Bit-space translation
 *
 * Protocol-bit numbers (0..15) are an arbitrary mapping defined by the
 * switching-board readme. Wire-bit numbers (0..15) follow the physical
 * shift-register order on the relay coils. Both are independent of the
 * main-board's RELAY_* enum — main owns its own assignment.
 *
 *   Wire 1 is the "main" power-master rail: asserted by hardware whenever
 *   any other relay is on. The master's relay_state writes for protocol
 *   bit 15 are honored as well, so an explicit set is also possible.
 * ============================================================================ */

#define WIRE_MAIN_BIT       1u
#define PROTO_MAIN_BIT      15u

static const uint8_t proto_to_wire[16] = {
    2,  /* 0  autopilot         */
    3,  /* 1  bow_light         */
    4,  /* 2  stern_light       */
    5,  /* 3  steaming_light    */
    6,  /* 4  anchor_light      */
    8,  /* 5  tricolor_light    */
    9,  /* 6  fresh_water_pump  */
    10, /* 7  fridge            */
    11, /* 8  inverter          */
    12, /* 9  aux1              */
    13, /* 10 aux2              */
    14, /* 11 deck_lights       */
    15, /* 12 usb               */
    7,  /* 13 cabin_lights      */
    0,  /* 14 instruments       */
    1,  /* 15 main              */
};

static const uint8_t wire_to_proto[16] = {
    14, /* wire 0  instruments       */
    15, /* wire 1  main              */
    0,  /* wire 2  autopilot         */
    1,  /* wire 3  bow_light         */
    2,  /* wire 4  stern_light       */
    3,  /* wire 5  steaming_light    */
    4,  /* wire 6  anchor_light      */
    13, /* wire 7  cabin_lights      */
    5,  /* wire 8  tricolor_light    */
    6,  /* wire 9  fresh_water_pump  */
    7,  /* wire 10 fridge            */
    8,  /* wire 11 inverter          */
    9,  /* wire 12 aux1              */
    10, /* wire 13 aux2              */
    11, /* wire 14 deck_lights       */
    12, /* wire 15 usb               */
};

static uint16_t map_proto_to_wire(uint16_t proto) {
    uint16_t wire = 0;
    for (uint8_t b = 0; b < 16; b++) {
        if (proto & (uint16_t)(1u << b)) wire |= (uint16_t)(1u << proto_to_wire[b]);
    }
    return wire;
}

static uint16_t map_wire_to_proto(uint16_t wire) {
    uint16_t proto = 0;
    for (uint8_t b = 0; b < 16; b++) {
        if (wire & (uint16_t)(1u << b)) proto |= (uint16_t)(1u << wire_to_proto[b]);
    }
    return proto;
}

/* ============================================================================
 * State shadow
 * ============================================================================ */

static volatile uint16_t relay_target;          /* protocol bits, last write */
static volatile uint16_t relay_physical;        /* protocol bits, from mux   */
static volatile uint16_t relay_mask;            /* protocol bits             */
static volatile uint8_t  level_mode_byte;       /* low nibble = packed mode  */
static volatile uint8_t  sensor_shadow;         /* current 3-bit sensor word */

static volatile uint16_t last_pushed_relays;
static volatile uint8_t  last_pushed_sensors;
static volatile uint8_t  push_dirty;

static volatile uint16_t battery_mv;
static volatile uint8_t  levels[2];

/* ============================================================================
 * Tasks
 * ============================================================================ */

#define MONITOR_TICK_MS   50u
#define POLL_TICK_MS      200u
#define RETRY_TICK_MS     TASK_MIN_MS

static void apply_target          (uint16_t proto_target);
static void on_sensors_changed    (uint8_t prev, uint8_t curr);
static void monitor_task          (TaskId id, void *ctx);
static void poll_battery_task     (TaskId id, void *ctx);
static void poll_levels_task      (TaskId id, void *ctx);
static void retry_task            (TaskId id, void *ctx);

void controller_init(TaskController *ctrl) {
    relay_target        = 0;
    relay_physical      = 0;
    relay_mask          = 0;
    level_mode_byte     = 0;
    sensor_shadow       = sensors_state();
    last_pushed_relays  = 0;
    last_pushed_sensors = sensor_shadow;
    push_dirty          = 0;
    battery_mv          = 0;
    levels[0]           = 0;
    levels[1]           = 0;

    apply_target(0);

    sensors_set_change_handler(on_sensors_changed);

    task_controller_add(ctrl, TASK_POLL_MONITOR, MONITOR_TICK_MS, monitor_task,      0);
    task_controller_add(ctrl, TASK_COMM_RETRY,   RETRY_TICK_MS,   retry_task,        0);
    task_controller_add(ctrl, TASK_POLL_BATTERY, POLL_TICK_MS,    poll_battery_task, 0);
    task_controller_add(ctrl, TASK_POLL_LEVELS,  POLL_TICK_MS,    poll_levels_task,  0);
}

/* ============================================================================
 * Inbound writes
 * ============================================================================ */

void controller_set_relay_target(uint16_t target) {
    relay_target = target;
    apply_target(target);
}

void controller_set_relay_mask(uint16_t mask) {
    relay_mask = mask;
}

void controller_set_level_mode(uint8_t mode_byte) {
    level_mode_byte = (uint8_t)(mode_byte & 0x0F);
}

/* ============================================================================
 * Queries
 * ============================================================================ */

uint16_t controller_relay_target  (void) { return relay_target; }
uint16_t controller_relay_physical(void) { return relay_physical; }
uint16_t controller_relay_mask    (void) { return relay_mask; }
uint8_t  controller_level_mode    (void) { return level_mode_byte; }
uint16_t controller_battery_mv    (void) { return battery_mv; }
uint8_t  controller_level         (uint8_t i) { return (i < 2) ? levels[i] : 0; }

/* ============================================================================
 * Internal
 * ============================================================================ */

static void apply_target(uint16_t proto_target) {
    uint16_t wire = map_proto_to_wire(proto_target);
    /* Power-master rail: assert wire bit 1 whenever any non-main relay is
     * on. The master can also drive proto bit 15 explicitly to force it. */
    if (wire & (uint16_t)~(1u << WIRE_MAIN_BIT)) wire |= (uint16_t)(1u << WIRE_MAIN_BIT);
    relay_out_write(wire);
}

static void on_sensors_changed(uint8_t prev, uint8_t curr) {
    (void)prev;
    INTERRUPT_PUSH;
    sensor_shadow = curr;
    push_dirty    = 1;
    INTERRUPT_POP;
}

static void monitor_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    uint16_t wire  = relay_mon_read();
    uint16_t proto = map_wire_to_proto(wire);

    INTERRUPT_PUSH;
    /* Always report wire-1 ("main") transitions even if its proto bit isn't
     * in the mask — the master needs to know the rail is hot. */
    uint16_t effective = (uint16_t)(relay_mask | (1u << PROTO_MAIN_BIT));
    if ((proto ^ relay_physical) & effective) push_dirty = 1;
    relay_physical = proto;
    INTERRUPT_POP;
}

static void poll_battery_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    battery_mv = adc_read_battery_mv();
}

static void poll_levels_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    levels[0] = adc_read_level_fresh_water();
    levels[1] = adc_read_level_fuel();
}

static void retry_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    if (!push_dirty) return;

    uint16_t prev_r, curr_r;
    uint8_t  prev_s, curr_s;
    {
        INTERRUPT_PUSH;
        prev_r = last_pushed_relays;
        curr_r = relay_physical;
        prev_s = last_pushed_sensors;
        curr_s = sensor_shadow;
        INTERRUPT_POP;
    }

    CommMessage msg;
    uint8_t len = comm_build_relay_changed(&msg, prev_r, curr_r, prev_s, curr_s);
    if (i2c_transmit(COMM_ADDRESS_MAIN, (const uint8_t *)&msg, len) != I2C_RESULT_OK) return;

    {
        INTERRUPT_PUSH;
        last_pushed_relays  = curr_r;
        last_pushed_sensors = curr_s;
        if (relay_physical == curr_r && sensor_shadow == curr_s) push_dirty = 0;
        INTERRUPT_POP;
    }
}
