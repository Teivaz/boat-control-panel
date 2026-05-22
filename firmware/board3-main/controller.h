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

/* Returns 1 when the value has not been updated for > 10 s (switching
 * board unresponsive). UI should show "error" instead of the reading. */
uint8_t controller_battery_stale(void);
uint8_t controller_levels_stale(void);
uint8_t controller_sensors_stale(void);

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

/* Async UI completions (main context). */
typedef void (*ControllerOpCompletion)(uint8_t ok, void* ctx);
typedef void (*ControllerReadCompletion)(uint8_t ok, uint8_t value, void* ctx);

/* Async read/write of any peer board's config byte (e.g. the per-meter
 * level offset calibration on the switching board, or per-side LED
 * brightness on a button board). Used by the menu UI to inspect and
 * adjust runtime parameters. Completion fires in main context.  Only
 * one config R/W op may be in flight at a time. */
void controller_read_config(uint8_t board_addr, uint8_t address, ControllerReadCompletion cb, void* ctx);
void controller_write_config(uint8_t board_addr, uint8_t address, uint8_t value, ControllerOpCompletion cb, void* ctx);

#endif /* CONTROLLER_H */
