/*
 * File:   interrupt.h
 * Author: teivaz
 *
 * Created on March 30, 2026, 11:18 PM
 */

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <xc.h>

void interrupt_init(void);
void interrupt_set_handler_IOC(void (*h)(void));
void interrupt_set_handler_TMR0(void (*h)(void));

#endif /* INTERRUPT_H */
