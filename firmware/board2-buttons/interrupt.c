#include "interrupt.h"

void interrupt_init(void)
{
    GIE = 0;

    IVTLOCK = 0x55;
    IVTLOCK = 0xAA;
    IVTLOCKbits.IVTLOCKED = 0x00; // unlock IVT

    IVTBASEU = 0;
    IVTBASEH = 0;
    IVTBASEL = 8;

    IVTLOCK = 0x55;
    IVTLOCK = 0xAA;
    IVTLOCKbits.IVTLOCKED = 0x01; // lock IVT

    // Assign peripheral interrupt priority vectors
    IPR1bits.INT0IP = 1;
    IPR3bits.TMR0IP = 1;

    GIE = 1;
}

void (*interrupt_handler_IOC)(void);
void (*interrupt_handler_TMR0)(void);

void interrupt_set_handler_IOC(void (* interrupt_handler)(void)){
    interrupt_handler_IOC = interrupt_handler;
}

void interrupt_set_handler_TMR0(void (* interrupt_handler)(void)){
    interrupt_handler_TMR0 = interrupt_handler;
}

void __interrupt(irq(IOC),base(8)) IOC_ISR()
{
    PIR0bits.IOCIF = 0; // EXT_INT0 Interrupt Flag Clear

    if(interrupt_handler_IOC)
    {
        interrupt_handler_IOC();
    }
}

void __interrupt(irq(TMR0), base(8)) TMR0_ISR()
{
    PIR3bits.TMR0IF = 0;
    
    if (interrupt_handler_TMR0)
    {
        interrupt_handler_TMR0();
    }
}
