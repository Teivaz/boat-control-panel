#include "nav_lights.h"

#include "config.h"

/* Each mode has a primary pattern and a fallback. The first pattern whose
 * lights are all enabled wins; otherwise the mode is unrealisable. */
typedef struct {
    uint8_t primary;
    uint8_t fallback;
} NavPlan;

static const NavPlan plans[] = {
    [NAV_MODE_OFF] = {0, 0},
    [NAV_MODE_ANCHORING] = {NAV_LIGHT_ANCHORING,
                            NAV_LIGHT_STEAMING | NAV_LIGHT_STERN},
    [NAV_MODE_STEAMING] = {NAV_LIGHT_STERN | NAV_LIGHT_BOW | NAV_LIGHT_STEAMING,
                           NAV_LIGHT_BOW | NAV_LIGHT_ANCHORING},
    [NAV_MODE_RUNNING] = {NAV_LIGHT_STERN | NAV_LIGHT_BOW, NAV_LIGHT_TRICOLOR},
};

NavResolution nav_lights_resolve(NavMode mode, uint8_t enabled_mask) {
    NavResolution r = {0, 0};
    if (mode == NAV_MODE_OFF) {
        return r;
    }

    const NavPlan p = plans[mode];
    if ((p.primary & enabled_mask) == p.primary) {
        r.lights_mask = p.primary;
        return r;
    }
    if ((p.fallback & enabled_mask) == p.fallback) {
        r.lights_mask = p.fallback;
        return r;
    }
    r.error = 1;
    return r;
}
