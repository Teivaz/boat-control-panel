#include "sensors.h"

#include "interrupt.h"
#include "libcomm.h"
#include "task_ids.h"

#include <xc.h>

/* IOC-driven debounce. Each on/off input runs an independent timer: the IOC
 * interrupt arms the timer on every edge, and the periodic task counts the
 * ticks the pin has remained at its new value. A change is committed only
 * when the pin stays put for THRESHOLD_MS, filtering bounce and short
 * glitches on the AC / shore / bilge harness. */

#define SENSORS_POLL_MS 5u
#define SENSORS_THRESHOLD_MS 50u
#define SENSORS_THRESHOLD_TICKS ((SENSORS_THRESHOLD_MS + SENSORS_POLL_MS - 1u) / SENSORS_POLL_MS)

#define PIN_BILGE PORTCbits.RC1
#define PIN_SHORE PORTCbits.RC2
#define PIN_AC PORTCbits.RC5

static volatile uint8_t stable_state;
static volatile uint8_t pending_ticks[3];
static volatile uint8_t pending_target[3];
static SensorsChangeHandler change_handler;

static uint8_t read_raw(void);
static void arm_pin(uint8_t i, uint8_t cur_bit);
static void ioc_handler(void);
static void poll_task(TaskId id, void* ctx);

void sensors_init(TaskController* ctrl) {
    /* RC5 can also act as BPEN output — BPEN=OFF in the config bits keeps
     * it free as a regular GPIO input. */
    ANSELCbits.ANSELC1 = 0;
    ANSELCbits.ANSELC2 = 0;
    ANSELCbits.ANSELC5 = 0;
    TRISCbits.TRISC1 = 1;
    TRISCbits.TRISC2 = 1;
    TRISCbits.TRISC5 = 1;

    stable_state = read_raw();
    for (uint8_t i = 0; i < 3; i++) {
        pending_ticks[i] = 0;
        pending_target[i] = (uint8_t)((stable_state >> i) & 1u);
    }

    IOCCP = 0;
    IOCCN = 0;
    IOCCF = 0;
    IOCCPbits.IOCCP1 = 1;
    IOCCPbits.IOCCP2 = 1;
    IOCCPbits.IOCCP5 = 1;
    IOCCNbits.IOCCN1 = 1;
    IOCCNbits.IOCCN2 = 1;
    IOCCNbits.IOCCN5 = 1;

    interrupt_set_handler_IOC(ioc_handler);
    PIE0bits.IOCIE = 1;

    task_controller_add(ctrl, TASK_POLL_SENSORS, SENSORS_POLL_MS, poll_task, 0);
}

uint8_t sensors_state(void) {
    return stable_state;
}

void sensors_set_change_handler(SensorsChangeHandler handler) {
    change_handler = handler;
}

static uint8_t read_raw(void) {
    uint8_t s = 0;
    if (PIN_BILGE) {
        s |= 0x01;
    }
    if (PIN_SHORE) {
        s |= 0x02;
    }
    if (PIN_AC) {
        s |= 0x04;
    }
    return s;
}

static void arm_pin(uint8_t i, uint8_t cur_bit) {
    uint8_t stable_bit = (uint8_t)((stable_state >> i) & 1u);
    if (cur_bit == stable_bit) {
        /* Reverted to the committed value before threshold elapsed — drop. */
        pending_ticks[i] = 0;
        pending_target[i] = cur_bit;
    } else {
        pending_target[i] = cur_bit;
        pending_ticks[i] = SENSORS_THRESHOLD_TICKS;
    }
}

static void ioc_handler(void) {
    /* Clear only the pins we own; leave any spurious flags on unused bits
     * alone. Edge flags are cleared before re-reading so a transition that
     * races the read still raises a fresh interrupt. */
    IOCCFbits.IOCCF1 = 0;
    IOCCFbits.IOCCF2 = 0;
    IOCCFbits.IOCCF5 = 0;
    uint8_t raw = read_raw();
    arm_pin(0, (uint8_t)(raw & 0x01u));
    arm_pin(1, (uint8_t)((raw >> 1) & 1u));
    arm_pin(2, (uint8_t)((raw >> 2) & 1u));
}

static void poll_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    uint8_t raw = read_raw();
    for (uint8_t i = 0; i < 3; i++) {
        if (pending_ticks[i] == 0) {
            continue;
        }
        uint8_t cur_bit = (uint8_t)((raw >> i) & 1u);
        uint8_t stable_bit = (uint8_t)((stable_state >> i) & 1u);
        if (cur_bit == stable_bit) {
            pending_ticks[i] = 0;
            continue;
        }
        if (cur_bit != pending_target[i]) {
            /* Mid-flight glitch — restart the window at the new value. */
            pending_target[i] = cur_bit;
            pending_ticks[i] = SENSORS_THRESHOLD_TICKS;
            continue;
        }

        uint8_t remaining;
        INTERRUPT_PUSH;
        remaining = (uint8_t)(pending_ticks[i] - 1u);
        pending_ticks[i] = remaining;
        INTERRUPT_POP;

        if (remaining == 0) {
            uint8_t prev = stable_state;
            uint8_t mask = (uint8_t)(1u << i);
            uint8_t next = (uint8_t)((stable_state & ~mask) | (uint8_t)(cur_bit << i));
            stable_state = next;
            if (change_handler) {
                change_handler(prev, next);
            }
        }
    }
}
