#ifndef LIBCOMM_H
#define LIBCOMM_H

#include <xc.h>

#define INTERRUPT_PUSH const int8_t _gie_state = GIE; GIE = 0
#define INTERRUPT_POP GIE = (__bit)_gie_state

/* ============================================================================
 * Device Addresses
 * ============================================================================ */

#define COMM_ADDRESS_MAIN              0x40
#define COMM_ADDRESS_SWITCHING         0x42
#define COMM_ADDRESS_BUTTON_BOARD_L    0x44
#define COMM_ADDRESS_BUTTON_BOARD_L2   0x45
#define COMM_ADDRESS_BUTTON_BOARD_R    0x46
#define COMM_ADDRESS_BUTTON_BOARD_R2   0x47
#define COMM_ADDRESS_RTC               0x68

/* ============================================================================
 * Command IDs
 * ============================================================================ */

typedef enum {
    /* Write IDs: MSB clear (0x00–0x7F) */
    COMM_BUTTON_EFFECT  = 0x01,  /* main -> button board */
    COMM_BUTTON_CHANGED = 0x02,  /* button board -> main */
    COMM_BUTTON_TRIGGER = 0x04,  /* main -> button board */
    COMM_RELAY_STATE    = 0x05,  /* main -> switching    */
    COMM_RELAY_CHANGED  = 0x06,  /* switching -> main    */
    COMM_RELAY_MASK     = 0x07,  /* main -> switching    */
    COMM_LEVEL_MODE     = 0x0A,  /* main -> switching    */

    /* Read IDs: MSB set (0x80–0xFF); `write ID | 0x80` when a write form exists */
    COMM_BUTTON_STATE_READ   = 0x83,                        /* main -> button board */
    COMM_BUTTON_TRIGGER_READ = COMM_BUTTON_TRIGGER | 0x80,  /* 0x84 */
    COMM_RELAY_STATE_READ    = COMM_RELAY_STATE    | 0x80,  /* 0x85 */
    COMM_RELAY_MASK_READ     = COMM_RELAY_MASK     | 0x80,  /* 0x87 */
    COMM_BATTERY_READ        = 0x88,                        /* main -> switching    */
    COMM_LEVELS_READ         = 0x89,                        /* main -> switching    */
    COMM_LEVEL_MODE_READ     = COMM_LEVEL_MODE     | 0x80,  /* 0x8A */
    COMM_SENSORS_READ        = 0x8B,                        /* main -> switching    */
} CommId;

/* ============================================================================
 * Button trigger mode (MM bits in MMEETTTT)
 * ============================================================================ */

typedef enum {
    COMM_BUTTON_MODE_UNKNOWN = 0x00,
    COMM_BUTTON_MODE_RELEASE = 0x01,  /* fire on release after >= t ms    */
    COMM_BUTTON_MODE_HOLD    = 0x02,  /* fire once after held for >= t ms */
    COMM_BUTTON_MODE_CHANGE  = 0x03,  /* fire on every state change       */
} CommButtonMode;

/* ============================================================================
 * Level meter mode (each bit of level_mode)
 * ============================================================================ */

typedef enum {
    COMM_METER_MODE_UNKNOWN = 0,
    COMM_METER_MODE_240_33  = 1,
    COMM_METER_MODE_0_190   = 2,
} CommMeterMode;

/* ============================================================================
 * Button output effect (button_effect nibble: CCMM)
 * ============================================================================ */

typedef enum {
    COMM_EFFECT_COLOR_WHITE = 0x00,
    COMM_EFFECT_COLOR_RED   = 0x01,
    COMM_EFFECT_COLOR_GREEN = 0x02,
    COMM_EFFECT_COLOR_BLUE  = 0x03,
} CommEffectColor;

typedef enum {
    COMM_EFFECT_MODE_DISABLED  = 0x00,
    COMM_EFFECT_MODE_ENABLED   = 0x01,
    COMM_EFFECT_MODE_FLASHING  = 0x02,  /* fast  */
    COMM_EFFECT_MODE_PULSATING = 0x03,  /* slow  */
} CommEffectMode;

/* ============================================================================
 * Message Payload Structs
 * ============================================================================ */

/**
 * MMEETTTT byte — packed trigger parameters.
 * t = TTTT x 10^EE ms.  TTTT=0 means immediate (t=0 ms).
 */
typedef struct {
    uint8_t time_mantissa : 4;  /* TTTT [3:0] */
    uint8_t time_exponent : 2;  /* EE   [5:4] */
    uint8_t mode          : 2;  /* MM   [7:6] */
} CommTriggerConfig;

/**
 * Per-output effect nibble (CCMM). sizeof == 1 — only [3:0] is meaningful.
 * Use `.raw` for wire-level access and the bitfield view for typed access.
 */
typedef union {
    uint8_t raw;
    struct {
        uint8_t mode  : 2;  /* CommEffectMode  MM [1:0] */
        uint8_t color : 2;  /* CommEffectColor CC [3:2] */
    };
} CommButtonOutputEffect;

/**
 * button_effect (0x01): 4 bytes on the wire.
 * Two outputs per byte; high-output bytes transmitted first.
 * Upper nibble = odd output, lower nibble = even output.
 * Use comm_button_effect_get/set to access individual outputs.
 */
typedef struct {
    uint8_t outputs_76;
    uint8_t outputs_54;
    uint8_t outputs_32;
    uint8_t outputs_10;
} CommButtonEffect;

/** button_changed (0x02): 3 bytes */
typedef struct {
    uint8_t device_address;
    uint8_t prev_state;
    uint8_t current_state;
} CommButtonChanged;

/** button_state_read (0x83) response: 1 byte */
typedef struct {
    uint8_t current_state;
} CommButtonState;

/** button_trigger (0x04) write payload: 2 bytes */
typedef struct {
    uint8_t           button_id;  /* [7:3]=0, [2:0]=id */
    CommTriggerConfig config;     /* MMEETTTT */
} CommButtonTrigger;

/**
 * relay_state (0x05) write/response payload: 2 bytes
 * Relay bitmask, little-endian: bit N = relay N; 1 = on.
 */
typedef struct {
    uint16_t relays;
} CommRelayState;

/**
 * relay_changed (0x06): 7 bytes
 * Relay state is little-endian 16-bit: low byte first on the wire.
 */
typedef struct {
    uint8_t  device_address;
    uint16_t prev_relays;     /* bit N = relay N */
    uint16_t current_relays;
    uint8_t  prev_sensors;    /* [7:3]=0, [2] s2, [1] s1, [0] s0 */
    uint8_t  current_sensors;
} CommRelayChanged;

/** relay_mask (0x07) write/response payload: 2 bytes */
typedef struct {
    uint16_t mask;  /* bit N = relay N; 1 = events enabled */
} CommRelayMask;

/** battery_read (0x88) response: 2 bytes */
typedef struct {
    uint16_t voltage;  /* little-endian unsigned */
} CommBattery;

/** levels_read (0x89) response: 2 bytes */
typedef struct {
    uint8_t level_0;
    uint8_t level_1;
} CommLevels;

/** level_mode (0x0A) write/response payload: 1 byte
 *  Two 2-bit fields, one per level meter. Each takes a CommMeterMode value. */
typedef struct {
    uint8_t mode_0 : 2;  /* level meter 0 */
    uint8_t mode_1 : 2;  /* level meter 1 */
} CommLevelMode;

/** sensors_read (0x8B) response: 1 byte */
typedef struct {
    uint8_t sensors;  /* [7:3]=0, [2] s2, [1] s1, [0] s0 */
} CommSensors;

/* ============================================================================
 * Universal Message Envelope
 * ============================================================================ */

/**
 * Universal message: command byte followed by the payload union.
 * Inspect id to determine which union member to access.
 */
typedef struct {
    uint8_t id;  /* CommId */
    union {
        CommButtonEffect  button_effect;   /* 0x01: 4 bytes */
        CommButtonChanged button_changed;  /* 0x02: 3 bytes */
        CommButtonState   button_state;    /* 0x83: 1 byte  */
        CommButtonTrigger button_trigger;  /* 0x04: 2 bytes */
        CommRelayState    relay_state;     /* 0x05: 2 bytes */
        CommRelayChanged  relay_changed;   /* 0x06: 7 bytes */
        CommRelayMask     relay_mask;      /* 0x07: 2 bytes */
        CommBattery       battery;         /* 0x88: 2 bytes */
        CommLevels        levels;          /* 0x89: 2 bytes */
        CommLevelMode     level_mode;      /* 0x0A: 1 byte  */
        CommSensors       sensors;         /* 0x8B: 1 byte  */
        uint8_t           raw[7];
    };
} CommMessage;

/* ============================================================================
 * Device address
 * ============================================================================ */

/** Returns the I2C address of this device, selected by DEVICE_TYPE_* macro. */
uint8_t comm_address(void);

/* ============================================================================
 * Outbound message builders
 *
 * Each builder fills *msg and returns the number of bytes (id + payload)
 * the caller must transmit. The caller owns the I2C transaction.
 * ============================================================================ */

/* button_effect (0x01) — main -> button board */
uint8_t comm_build_button_effect(CommMessage *msg, const CommButtonEffect *effect);

/* button_changed (0x02) — button board -> main; device_address is filled from comm_address() */
uint8_t comm_build_button_changed(CommMessage *msg, uint8_t prev_state, uint8_t current_state);

/* button_state_read (0x83) — main -> button board */
uint8_t comm_build_button_state_read(CommMessage *msg);

/* button_trigger (0x04) — main -> button board */
uint8_t comm_build_button_trigger(CommMessage *msg, uint8_t button_id, CommTriggerConfig config);

/* button_trigger_read (0x84) — main -> button board */
uint8_t comm_build_button_trigger_read(CommMessage *msg, uint8_t button_id);

/* relay_state (0x05) — main -> switching board */
uint8_t comm_build_relay_state(CommMessage *msg, uint16_t relays);

/* relay_state_read (0x85) — main -> switching board */
uint8_t comm_build_relay_state_read(CommMessage *msg);

/* relay_changed (0x06) — switching board -> main; device_address is filled from comm_address() */
uint8_t comm_build_relay_changed(CommMessage *msg,
                                 uint16_t prev_relays, uint16_t current_relays,
                                 uint8_t prev_sensors, uint8_t current_sensors);

/* relay_mask (0x07) — main -> switching board */
uint8_t comm_build_relay_mask(CommMessage *msg, uint16_t mask);

/* relay_mask_read (0x87) — main -> switching board */
uint8_t comm_build_relay_mask_read(CommMessage *msg);

/* battery_read (0x88) — main -> switching board */
uint8_t comm_build_battery_read(CommMessage *msg);

/* levels_read (0x89) — main -> switching board */
uint8_t comm_build_levels_read(CommMessage *msg);

/* level_mode (0x0A) — main -> switching board */
uint8_t comm_build_level_mode(CommMessage *msg, CommMeterMode mode_0, CommMeterMode mode_1);

/* level_mode_read (0x8A) — main -> switching board */
uint8_t comm_build_level_mode_read(CommMessage *msg);

/* sensors_read (0x8B) — main -> switching board */
uint8_t comm_build_sensors_read(CommMessage *msg);

/* ============================================================================
 * Inbound payload parsers
 *
 * `data` points at the first payload byte (the cmd id is consumed by the
 * caller's dispatch). Endianness conversion and reserved-bit masking happen here.
 * ============================================================================ */

void comm_parse_button_effect(const uint8_t *data, CommButtonEffect *effect);
void comm_parse_button_changed(const uint8_t *data, CommButtonChanged *event);
void comm_parse_button_state_response(const uint8_t *data, CommButtonState *state);
void comm_parse_button_trigger_write(const uint8_t *data, CommButtonTrigger *trigger);
void comm_parse_button_trigger_response(const uint8_t *data, CommTriggerConfig *config);
void comm_parse_relay_state_write(const uint8_t *data, CommRelayState *state);
void comm_parse_relay_state_response(const uint8_t *data, CommRelayState *state);
void comm_parse_relay_changed(const uint8_t *data, CommRelayChanged *event);
void comm_parse_relay_mask_write(const uint8_t *data, CommRelayMask *mask);
void comm_parse_relay_mask_response(const uint8_t *data, CommRelayMask *mask);
void comm_parse_battery_response(const uint8_t *data, CommBattery *battery);
void comm_parse_levels_response(const uint8_t *data, CommLevels *levels);
void comm_parse_level_mode_write(const uint8_t *data, CommLevelMode *mode);
void comm_parse_level_mode_response(const uint8_t *data, CommLevelMode *mode);
void comm_parse_sensors_response(const uint8_t *data, CommSensors *sensors);

/* ============================================================================
 * button_effect helpers
 * ============================================================================ */

void   comm_button_effect_init(CommButtonEffect *effect);
int8_t comm_button_effect_set(CommButtonEffect *effect, uint8_t output_index, CommButtonOutputEffect value);
int8_t comm_button_effect_get(const CommButtonEffect *effect, uint8_t output_index, CommButtonOutputEffect *value);

/* ============================================================================
 * button_trigger helpers
 * ============================================================================ */

/** Decode TTTT x 10^EE into milliseconds. Max value: 15000. */
uint16_t comm_button_trigger_time_ms(CommTriggerConfig config);

/** Build a config from mode + time in ms. time_ms is clamped to 15000 and
 *  encoded with the smallest EE that fits (finest available resolution). */
CommTriggerConfig comm_button_trigger_make(CommButtonMode mode, uint16_t time_ms);

#endif /* LIBCOMM_H */
