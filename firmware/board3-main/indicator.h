#ifndef INDICATOR_H
#define INDICATOR_H

#include "task.h"

/* Five RGB LEDs on RB5 showing the live state of the navigation-light relays.
 * Each LED maps to one of the five channels (anchoring / tricolor / steaming
 * / bow / stern). A white pixel indicates the light is active on the
 * switching board; LEDs flash red whenever the controller reports an
 * unrealisable mode. */
void indicator_init(TaskController* ctrl);

#endif /* INDICATOR_H */
