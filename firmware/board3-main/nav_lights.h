#ifndef NAV_LIGHTS_H
#define NAV_LIGHTS_H

#include <stdint.h>

/* Operating mode requested by the user. */
typedef enum {
    NAV_MODE_OFF       = 0,
    NAV_MODE_ANCHORING = 1,
    NAV_MODE_STEAMING  = 2,
    NAV_MODE_RUNNING   = 3,
} NavMode;

/* Result of resolving a mode against the available (enabled) lights. */
typedef struct {
    uint8_t lights_mask;   /* bitmask using NAV_LIGHT_* from config.h          */
    uint8_t error;         /* 1 = requested mode cannot be realised            */
} NavResolution;

/* Resolves the desired mode into concrete lights given which of the five
 * physical lights are present (`enabled_mask`). Pure function; no state. */
NavResolution nav_lights_resolve(NavMode mode, uint8_t enabled_mask);

#endif /* NAV_LIGHTS_H */
