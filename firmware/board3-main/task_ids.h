#ifndef TASK_IDS_H
#define TASK_IDS_H

enum {
    TASK_COMM_RETRY,   // drain pending outbound i2c writes
    TASK_CONFIG_FLUSH, // commit deferred EEPROM writes
    TASK_POLL_BATTERY, // battery_read from switching board
    TASK_POLL_LEVELS,  // levels_read from switching board
    TASK_POLL_SENSORS, // sensors_read from switching board
    TASK_POLL_RTC,     // DS3231 time read
    TASK_NAV_LIGHTS,   // nav-light mode tick (flashing error, etc.)
    TASK_CONFIG_MODE,  // sample RA7 config-mode switch
};

#endif /* TASK_IDS_H */
