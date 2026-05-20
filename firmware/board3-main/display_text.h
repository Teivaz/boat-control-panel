#ifndef DISPLAY_TEXT_H
#define DISPLAY_TEXT_H

#include "task.h"

#include <stdint.h>

/* Text UI on top of the SSD1322 parallel driver.
 *
 * Normal mode: three centred lines showing fresh water level (0..100%), fuel
 * level (0..100%), and battery voltage (e.g. 12.6 V).
 *
 * Config mode: five columns labelled A T S B R (anchoring, tricolor,
 * steaming, bow, stern); each column is either filled (enabled) or empty
 * (disabled). The active cursor slot is shown highlighted.
 *
 * The OLED panel itself is left in power-save after init — the rendered
 * content goes to DDRAM but no pixels are visible — until a caller asks
 * for it via display_text_set_active(1). The periodic refresh task only
 * paints frames while the panel is active. */
void display_text_init(TaskController* ctrl);

/* Drive the OLED panel on or off. Idempotent.
 *   on=1 : take the SSD1322 out of power-save. The next refresh_task
 *          tick (≤ 250 ms) paints the current frame; until then DDRAM
 *          shows whatever was last rendered before the previous off.
 *   on=0 : put the SSD1322 back into power-save. DDRAM and the host
 *          framebuffer are left as-is — refresh_task early-returns
 *          while inactive so they won't be touched until the next
 *          on=1. */
void display_text_set_active(uint8_t on);

#endif /* DISPLAY_TEXT_H */
