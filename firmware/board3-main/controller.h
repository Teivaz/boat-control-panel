#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "libcomm.h"
#include "nav_lights.h"
#include "rtc.h"
#include "task.h"

#include <stdint.h>

/* Registers the main-board control logic: inbound event hooks, periodic
 * relay-sync task, and outbound retry queue. */
void controller_init(TaskController* ctrl);

/* Inbound dispatch hooks. Both are invoked from I2C ISR context — must be
 * non-blocking. Senders are identified by their 7-bit I2C address. */
void controller_on_button_changed(uint8_t sender, uint8_t button_id, uint8_t pressed, CommButtonMode mode);
void controller_on_relay_changed(uint8_t sender, uint16_t prev_relays, uint16_t curr_relays, uint8_t prev_sensors,
                                 uint8_t curr_sensors);

/* Adopter-callback forwarders — called from comm.c when protocol read
 * responses arrive.  Main-loop context.  NULL pointer means I2C error. */
void controller_on_battery_response(const CommBattery* battery);
void controller_on_levels_response(const CommLevels* levels);
void controller_on_sensors_response(const CommSensors* sensors);
void controller_on_config_read_response(const uint8_t* value);

/* State queries for UI / display layers. */
uint8_t controller_power_on(void);
NavMode controller_nav_mode(void);
uint8_t controller_nav_error(void);
uint16_t controller_relay_target(void);
uint16_t controller_relay_physical(void);
uint16_t controller_battery_mv(void);
uint8_t controller_level(uint8_t meter_index); /* 0 or 1 */
uint8_t controller_sensors(void);

/* Returns 1 when the value has not been updated for > 10 s (switching
 * board unresponsive).  UI should show "error" instead of the reading. */
uint8_t controller_battery_stale(void);
uint8_t controller_levels_stale(void);
uint8_t controller_sensors_stale(void);

uint8_t controller_button_base_on(uint8_t side, uint8_t button_idx);

/* Last time read from the DS3231. Returns 1 if the shadow has been
 * populated by at least one successful poll, 0 otherwise. */
uint8_t controller_time(RtcTime* out);

/* Async UI completions (main context). */
typedef void (*ControllerOpCompletion)(uint8_t ok, void* ctx);
typedef void (*ControllerReadCompletion)(uint8_t ok, uint8_t value, void* ctx);

/* Push hour:minute to the RTC, refresh the shadow on success. cb fires in
 * main context with ok=1 on success, 0 on I2C failure. */
void controller_set_time(uint8_t hour, uint8_t minute, ControllerOpCompletion cb, void* ctx);

/* Async read/write of a switching-board config byte (e.g. the level-meter
 * offsets at CONFIG_ADDR_LEVEL_OFFSET_WATER / _FUEL). Used by the menu UI
 * to calibrate the float meters at runtime. Completion fires in main
 * context. */
void controller_read_switching_config(uint8_t address, ControllerReadCompletion cb, void* ctx);
void controller_write_switching_config(uint8_t address, uint8_t value, ControllerOpCompletion cb, void* ctx);

#endif /* CONTROLLER_H */
