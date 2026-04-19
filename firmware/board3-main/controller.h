#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "nav_lights.h"
#include "task.h"

#include <stdint.h>

/* Registers the main-board control logic: inbound event hooks, periodic
 * relay-sync task, and outbound retry queue. */
void controller_init(TaskController *ctrl);

/* Inbound dispatch hooks. Both are invoked from I2C ISR context — must be
 * non-blocking. Senders are identified by their 7-bit I2C address. */
void controller_on_button_changed(uint8_t sender, uint8_t prev, uint8_t curr);
void controller_on_relay_changed(uint8_t sender, uint16_t prev_relays,
                                 uint16_t curr_relays, uint8_t prev_sensors,
                                 uint8_t curr_sensors);

/* State queries for UI / display layers. */
uint8_t controller_power_on(void);
NavMode controller_nav_mode(void);
uint8_t controller_nav_error(void);
uint16_t controller_relay_target(void);
uint16_t controller_relay_physical(void);
uint16_t controller_battery_mv(void);
uint8_t controller_level(uint8_t meter_index); /* 0 or 1 */
uint8_t controller_sensors(void);

#endif /* CONTROLLER_H */
