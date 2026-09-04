/*
 * adopter_stubs.h — the "board" that libcomm_interface.c dispatches into.
 *
 * libcomm_interface.c is written against a set of comm_on_* callbacks each
 * board implements.  In the libcomm test binary there is no board, so this
 * file provides them as recorders: every callback appends to a log the test
 * can then assert on.  That makes the dispatcher testable end to end — a
 * frame goes in through the fake driver and the test checks which callback
 * came out, with which arguments.
 */

#ifndef ADOPTER_STUBS_H
#define ADOPTER_STUBS_H

#include "libcomm.h"
#include "libcomm_interface.h"

#include <stdint.h>

typedef enum {
    CB_NONE = 0,
    CB_RESET,
    CB_CONFIG_RECEIVED,
    CB_BUTTON_EFFECT_RECEIVED,
    CB_BUTTON_CHANGED_RECEIVED,
    CB_BUTTON_TRIGGER_RECEIVED,
    CB_RELAY_STATE_RECEIVED,
    CB_CHANNEL_CHANGED_RECEIVED,
    CB_LEVEL_MODE_RECEIVED,

    CB_RESET_COMPLETION,
    CB_CONFIG_COMPLETION,
    CB_BUTTON_EFFECT_COMPLETION,
    CB_BUTTON_CHANGED_COMPLETION,
    CB_BUTTON_TRIGGER_COMPLETION,
    CB_RELAY_STATE_COMPLETION,
    CB_CHANNEL_CHANGED_COMPLETION,
    CB_LEVEL_MODE_COMPLETION,

    CB_BUTTON_STATE_RESPONSE,
    CB_BUTTON_TRIGGER_RESPONSE,
    CB_RELAY_STATE_RESPONSE,
    CB_CHANNEL_STATE_RESPONSE,
    CB_BATTERY_RESPONSE,
    CB_LEVELS_RESPONSE,
    CB_LEVEL_MODE_RESPONSE,
    CB_SENSORS_RESPONSE,
    CB_CONFIG_RESPONSE,

    CB_BUTTON_STATE_REQUESTED,
    CB_BUTTON_TRIGGER_REQUESTED,
    CB_RELAY_STATE_REQUESTED,
    CB_CHANNEL_STATE_REQUESTED,
    CB_BATTERY_REQUESTED,
    CB_LEVELS_REQUESTED,
    CB_LEVEL_MODE_REQUESTED,
    CB_SENSORS_REQUESTED,
    CB_CONFIG_REQUESTED,
} CallbackKind;

typedef struct {
    CallbackKind kind;
    I2cResult result;
    uint8_t addr;
    uint8_t had_payload; /* 0 when the dispatcher passed NULL (failure path) */
    uint8_t u8[4];
    uint16_t u16[2];
} CallbackRecord;

#define ADOPTER_LOG_MAX 32

extern CallbackRecord adopter_log[ADOPTER_LOG_MAX];
extern uint8_t adopter_log_count;

/* Payload the read-request stubs hand to comm_respond().  Set before
 * delivering a read request to control what the device replies with; a
 * length of 0 makes the stub decline to respond, which is how a board
 * signals "no data" and gets the read-phase address NACKed. */
extern uint8_t adopter_response[8];
extern uint8_t adopter_response_len;

void adopter_reset(void);

/* First record of kind `k`, or NULL. */
const CallbackRecord* adopter_find(CallbackKind k);
uint8_t adopter_count_of(CallbackKind k);

#endif /* ADOPTER_STUBS_H */
