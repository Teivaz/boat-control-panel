#ifndef DISPLAY_H
#define DISPLAY_H

#include "u8g2.h"

/* SSD1322 monochrome 256x64 panel on an 8080-mode parallel bus (nCS=GND,
 * !RD pulled up — only D/!C, !RST, !WR, and D0..D7 are driven). The low
 * level pin/byte primitives live in display.c and are wrapped as u8g2
 * byte and gpio/delay callbacks so all rendering goes through u8g2. */

#define DISPLAY_WIDTH 256
#define DISPLAY_HEIGHT 64

void display_init(void);
u8g2_t* display_u8g2(void);

#endif /* DISPLAY_H */
