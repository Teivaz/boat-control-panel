#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include "task.h"

#include <stdint.h>

/* RA7 is wired to a momentary / toggle switch pulling the input low when the
 * user wants to enter configuration. While active, the normal button
 * dispatch is bypassed and the panel becomes a small menu UI:
 *
 *   Top-level menu -> Nav-light enable mask  (5 slots, toggle ON/OFF)
 *                  -> Set time               (HH:MM, written back to RTC)
 *                  -> Water-float offset     (raw 8-bit, written to switching)
 *                  -> Fuel-float  offset     (raw 8-bit, written to switching)
 *
 * Buttons (regardless of which screen is active):
 *   left  1  : up   / increment
 *   left  2  : down / decrement
 *   right 0  : back (returns to menu, or exits config when at the menu)
 *   right 1  : select
 *
 * Toggling RA7 inactive commits the working nav-light mask to EEPROM. Time
 * and float-offset edits commit on select. */

typedef enum {
    CONFIG_SCREEN_MENU = 0,
    CONFIG_SCREEN_NAV,
    CONFIG_SCREEN_TIME,
    CONFIG_SCREEN_OFFSET,
} ConfigScreen;

typedef enum {
    CONFIG_MENU_NAV = 0,
    CONFIG_MENU_TIME,
    CONFIG_MENU_OFFSET_WATER,
    CONFIG_MENU_OFFSET_FUEL,
    CONFIG_MENU_COUNT,
} ConfigMenuItem;

void config_mode_init(TaskController* ctrl);

uint8_t config_mode_active(void);
uint8_t config_mode_screen(void); /* ConfigScreen */
uint8_t config_mode_menu_cursor(void);
uint8_t config_mode_nav_cursor(void); /* index 0..4 within the nav screen */
uint8_t config_mode_nav_working_mask(void);
uint8_t config_mode_time_field(void); /* 0 = hour, 1 = minute */
uint8_t config_mode_time_hour(void);
uint8_t config_mode_time_minute(void);
uint8_t config_mode_offset_target(void); /* 0 = water, 1 = fuel */
uint8_t config_mode_offset_value(void);

/* Invoked from the button-change dispatch when config mode is active.
 * `side` is the originating button board I2C address; `button_idx` is the
 * index (0..6) of the button whose trigger fired. */
void config_mode_on_button_pressed(uint8_t side, uint8_t button_idx);

#endif /* CONFIG_MODE_H */
