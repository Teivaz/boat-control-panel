#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/* SSD1322 monochrome 256x64 panel on an 8080-mode parallel bus (nCS=GND,
 * !RD pulled up — only D/!C, !RST, !WR, and D0..D7 are driven). This
 * module configures the pins and exposes byte-level command/data write
 * primitives that a u8g2 byte callback can sit on top of. */

#define DISPLAY_WIDTH 256
#define DISPLAY_HEIGHT 64

void display_init(void); /* pin config + reset + SSD1322 boot sequence */
void display_send_cmd(uint8_t cmd);
void display_send_data(const uint8_t* data, uint16_t len);

#endif /* DISPLAY_H */
