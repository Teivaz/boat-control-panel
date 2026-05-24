#ifndef NAV_LIGHTS_H
#define NAV_LIGHTS_H

#include "indicator.h"
#include <stdint.h>

/* Operating mode requested by the user. */
typedef enum {
    NAV_LIGHTS_MODE_OFF = 0,
    NAV_LIGHTS_MODE_ANCHORING = 1,
    NAV_LIGHTS_MODE_STEAMING = 2,
    NAV_LIGHTS_MODE_RUNNING = 3,
} NavLightsMode;

/* Result of resolving a mode against the available (enabled) lights. */
typedef struct {
    NavLights lights_mask; // bitmask using NAV_LIGHT_* from config.h
    uint8_t error;         // 1 = requested mode cannot be realised
} NavResolution;

/* Resolves the desired mode into concrete lights given which of the five
 * physical lights are present (`available`). Pure function; no state. */
NavResolution nav_lights_resolve(NavLightsMode mode, NavLights available);

#endif /* NAV_LIGHTS_H */
