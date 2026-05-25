#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "button_fx.h" /* ButtonIndex / Channel for the public queries below */
#include "indicator.h" /* NavLights for the nav-state queries below */
#include "libcomm.h"
#include "task.h"

#include <stdint.h>

/* Registers the main-board control logic: inbound event hooks, periodic
 * relay-sync task, polling tasks, and indicator sync. */
void controller_init(TaskController* ctrl);

/* State queries for the display / button-fx layers. */
uint8_t controller_power_on(void);
uint16_t controller_battery_mv(void);
uint8_t controller_level(uint8_t meter_index); /* 0 = water, 1 = fuel */
uint8_t controller_sensors(void);

/* Nav-light state queries — the indicator's refresh task pulls these
 * each frame to decide what to render. All are O(1).
 *   enabled       — slots the user wants lit (target nav bits).
 *   pending       — enabled ∧ not-yet-observed.
 *   errored       — per-light failure (currently always 0; reserved for
 *                   the per-light timeout that doesn't exist yet).
 *   config_error  — 1 when the requested mode can't be realised with
 *                   the operator's nav-enabled mask. */
NavLights controller_nav_enabled(void);
NavLights controller_nav_pending(void);
NavLights controller_nav_errored(void);
uint8_t controller_nav_config_error(void);

#endif /* CONTROLLER_H */
