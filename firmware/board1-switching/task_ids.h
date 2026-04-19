#ifndef TASK_IDS_H
#define TASK_IDS_H

enum {
    TASK_COMM_RETRY,       /* push pending relay_changed to main                */
    TASK_CONFIG_FLUSH,     /* commit deferred EEPROM writes                     */
    TASK_POLL_MONITOR,     /* read relay physical state via mux, detect changes */
    TASK_POLL_SENSORS,     /* debounce bilge / shore / AC inputs                */
    TASK_POLL_BATTERY,     /* sample battery rail and update shadow             */
    TASK_POLL_LEVELS,      /* sample fresh-water + fuel level meters            */
};

#endif /* TASK_IDS_H */
