#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include "task.h"

#include <stdint.h>

/* RA7 is wired to a momentary / toggle switch pulling the input low when the
 * user wants to enter configuration. While active, the normal button
 * dispatch is bypassed and the panel becomes a small menu UI:
 *
 *   Top-level menu  -> Nav-light enable mask  (5 slots, toggle ON/OFF)
 *                   -> Edit byte values       (calibration / brightness / mode)
 *
 * Buttons (regardless of which screen is active):
 *   left 1 : up    / increment
 *   left 2 : down  / decrement
 *   left 3 : enter / select / commit
 *   left 4 : exit  / back / cancel
 *
 * Edit flow: entering a value item kicks off a read from the target board.
 * Up/down adjust a working value; enter writes it back and returns to the
 * menu on success; exit returns to the menu without committing. Toggling
 * RA7 inactive commits the working nav-light mask to EEPROM. */

typedef enum {
    CONFIG_SCREEN_MENU = 0,
    CONFIG_SCREEN_NAV,
    CONFIG_SCREEN_EDIT,
} ConfigScreen;

typedef enum {
    CONFIG_MENU_NAV = 0,
    CONFIG_MENU_WATER_CAL,
    CONFIG_MENU_FUEL_CAL,
    CONFIG_MENU_BATTERY_CAL,
    CONFIG_MENU_WATER_MODE,
    CONFIG_MENU_FUEL_MODE,
    CONFIG_MENU_BRIGHTNESS_L,
    CONFIG_MENU_BRIGHTNESS_R,
    CONFIG_MENU_BRIGHTNESS_IND,
    CONFIG_MENU_COUNT,
} ConfigMenuItem;

typedef enum {
    MENU_CONTROL_NEXT,
    MENU_CONTROL_PREV,
    MENU_CONTROL_ENTER,
    MENU_CONTROL_EXIT,
} MenuControl;

void config_mode_init(TaskController* ctrl);

uint8_t config_mode_active(void);
uint8_t config_mode_screen(void); /* ConfigScreen */
uint8_t config_mode_menu_cursor(void);
uint8_t config_mode_nav_cursor(void); /* index 0..4 within the nav screen */
uint8_t config_mode_nav_working_mask(void);

/* Edit-screen queries. menu_item identifies which value is being edited
 * (one of the byte/mode CONFIG_MENU_* values).  loaded is 0 between
 * entering the screen and the initial read response; 1 once the working
 * value has been populated.  value is the current working value
 * (0..255 for byte items; 0..2 for level-mode items). */
uint8_t config_mode_edit_menu_item(void);
uint8_t config_mode_edit_loaded(void);
uint8_t config_mode_edit_value(void);

/* Drive the menu state machine. Called from the controller's button
 * dispatch when config mode is active; no-ops when inactive. Main-loop
 * context only. */
void config_mode_on_action(MenuControl control);

#endif /* CONFIG_MODE_H */
