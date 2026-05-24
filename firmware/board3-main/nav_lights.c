#include "nav_lights.h"

#include "config.h"

/* Each mode has a primary pattern and a fallback. The first pattern whose
 * lights are all enabled wins; otherwise the mode is unrealisable. */
typedef struct {
    NavLights primary;
    NavLights fallback;
} NavPlan;

static const NavPlan k_nav_plan[] = {
    [NAV_LIGHTS_MODE_OFF] = {0, 0},
    [NAV_LIGHTS_MODE_ANCHORING] = {(NavLights){.anchoring=1}, (NavLights){.steaming=1, .stern=1}},
    [NAV_LIGHTS_MODE_STEAMING] = {(NavLights){.stern=1, .bow=1, .steaming=1}, (NavLights){.bow=1, .anchoring=1}},
    [NAV_LIGHTS_MODE_RUNNING] = {(NavLights){.tricolor=1}, (NavLights){.stern=1, .bow=1}},
};

NavResolution nav_lights_resolve(NavLightsMode mode, NavLights available) {
    NavResolution r = {0, 0};
    if (mode == NAV_LIGHTS_MODE_OFF) {
        return r;
    }

    const NavPlan p = k_nav_plan[mode];
    if ((p.primary.raw & available.raw) == p.primary.raw) {
        r.lights_mask = p.primary;
        return r;
    }
    if ((p.fallback.raw & available.raw) == p.fallback.raw) {
        r.lights_mask = p.fallback;
        return r;
    }
    r.error = 1;
    return r;
}
