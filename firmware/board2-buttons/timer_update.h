/* 
 * File:   timer_update.h
 * Author: teivaz
 *
 * Created on March 31, 2026, 1:28 AM
 */

#ifndef TIMER_UPDATE_H
#define	TIMER_UPDATE_H

#include <xc.h>

void timer_update_init(void);
void timer_update_set_time(uint8_t time);
void timer_update_set_callback(void (* interrupt_handler)(void));

#endif	/* TIMER_UPDATE_H */

