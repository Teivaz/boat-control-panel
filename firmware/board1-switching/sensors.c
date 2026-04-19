#include "sensors.h"

#include "task_ids.h"

#include <xc.h>

/* Poll interval and the number of consecutive matching samples required to
 * accept a new state. 3 samples at 20 ms ≈ 60 ms of debounce, which is
 * comfortably longer than any plausible bounce on the AC/shore-power relays
 * we're observing. */
#define SENSORS_POLL_MS 20u
#define SENSORS_STABLE_N 3u

#define PIN_BILGE PORTCbits.RC1
#define PIN_SHORE PORTCbits.RC2
#define PIN_AC PORTCbits.RC5

static volatile uint8_t stable_state;
static uint8_t candidate;
static uint8_t match_count;
static SensorsChangeHandler change_handler;

static uint8_t read_raw(void);
static void poll_task(TaskId id, void *ctx);

void sensors_init(TaskController *ctrl) {
    /* Digital inputs, no pullups (upstream circuitry provides the level).
     * RC5 is also a candidate CRC-on-boot output — we keep BPEN=OFF in the
     * config bits so it stays a free GPIO. */
    ANSELCbits.ANSELC1 = 0;
    ANSELCbits.ANSELC2 = 0;
    ANSELCbits.ANSELC5 = 0;
    TRISCbits.TRISC1 = 1;
    TRISCbits.TRISC2 = 1;
    TRISCbits.TRISC5 = 1;

    stable_state = read_raw();
    candidate = stable_state;
    match_count = SENSORS_STABLE_N;

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
    if (PIN_BILGE)
        s |= 0x01;
    if (PIN_SHORE)
        s |= 0x02;
    if (PIN_AC)
        s |= 0x04;
    return s;
}

static void poll_task(TaskId id, void *ctx) {
    (void) id;
    (void) ctx;
    uint8_t raw = read_raw();
    if (raw != candidate) {
        candidate = raw;
        match_count = 1;
        return;
    }
    if (match_count < SENSORS_STABLE_N) {
        match_count++;
        if (match_count == SENSORS_STABLE_N && raw != stable_state) {
            uint8_t prev = stable_state;
            stable_state = raw;
            if (change_handler)
                change_handler(prev, raw);
        }
    }
}
