#ifndef DISPLAY_TEXT_H
#define DISPLAY_TEXT_H

#include "task.h"

/* Text UI on top of the SSD1322 parallel driver.
 *
 * Normal mode: three centred lines showing fresh water level (0..100%), fuel
 * level (0..100%), and battery voltage (e.g. 12.6 V).
 *
 * Config mode: five columns labelled A T S B R (anchoring, tricolor,
 * steaming, bow, stern); each column is either filled (enabled) or empty
 * (disabled). The active cursor slot is shown highlighted.
 *
 * A periodic task refreshes the display every REFRESH_MS; calls only need to
 * re-issue the driver's command/data writes, no dynamic allocation. */
void display_text_init(TaskController* ctrl);

#endif /* DISPLAY_TEXT_H */
