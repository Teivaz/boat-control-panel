#include "interrupt.h"

void interrupt_init(void) {
    GIE = 0;

    IVTLOCK = 0x55;
    IVTLOCK = 0xAA;
    IVTLOCKbits.IVTLOCKED = 0x00;

    IVTBASEU = 0;
    IVTBASEH = 0;
    IVTBASEL = 8;

    IVTLOCK = 0x55;
    IVTLOCK = 0xAA;
    IVTLOCKbits.IVTLOCKED = 0x01;

    IPR3bits.TMR0IP = 1;

    GIE = 1;
}

void (*interrupt_handler_IOC)(void);
void (*interrupt_handler_TMR0)(void);

void interrupt_set_handler_IOC(void (*h)(void)) {
    interrupt_handler_IOC = h;
}
void interrupt_set_handler_TMR0(void (*h)(void)) {
    interrupt_handler_TMR0 = h;
}

void __interrupt(irq(IOC), base(8)) IOC_ISR(void) {
    PIR0bits.IOCIF = 0;
    if (interrupt_handler_IOC)
        interrupt_handler_IOC();
}

void __interrupt(irq(TMR0), base(8)) TMR0_ISR(void) {
    PIR3bits.TMR0IF = 0;
    if (interrupt_handler_TMR0)
        interrupt_handler_TMR0();
}

/* Unused vectors reset the device. I2C1 ISRs are defined in i2c.c and the
 * AD ISR is defined in adc.c. */
void __interrupt(irq(SWINT), base(8)) SWINT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(HLVD), base(8)) HLVD_ISR() {
    __asm("RESET");
}
void __interrupt(irq(OSF), base(8)) OSF_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CSW), base(8)) CSW_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TU16A), base(8)) TU16A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC1), base(8)) CLC1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CAN), base(8)) CAN_ISR() {
    __asm("RESET");
}
void __interrupt(irq(INT0), base(8)) INT0_ISR() {
    __asm("RESET");
}
void __interrupt(irq(ZCD), base(8)) ZCD_ISR() {
    __asm("RESET");
}
void __interrupt(irq(ACT), base(8)) ACT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CM1), base(8)) CM1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SMT1), base(8)) SMT1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SMT1PRA), base(8)) SMT1PRA_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SMT1PWA), base(8)) SMT1PWA_ISR() {
    __asm("RESET");
}
void __interrupt(irq(ADCH1), base(8)) ADCH1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(ADCH2), base(8)) ADCH2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(ADCH3), base(8)) ADCH3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(ADCH4), base(8)) ADCH4_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA1SCNT), base(8)) DMA1SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA1DCNT), base(8)) DMA1DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA1OR), base(8)) DMA1OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA1A), base(8)) DMA1A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SPI1RX), base(8)) SPI1RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SPI1TX), base(8)) SPI1TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SPI1), base(8)) SPI1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR2), base(8)) TMR2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR1), base(8)) TMR1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR1G), base(8)) TMR1G_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CCP1), base(8)) CCP1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U1RX), base(8)) U1RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U1TX), base(8)) U1TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U1E), base(8)) U1E_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U1), base(8)) U1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CANRX), base(8)) CANRX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CANTX), base(8)) CANTX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM1PR), base(8)) PWM1PR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM1), base(8)) PWM1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SPI2RX), base(8)) SPI2RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SPI2TX), base(8)) SPI2TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SPI2), base(8)) SPI2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TU16B), base(8)) TU16B_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR3), base(8)) TMR3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR3G), base(8)) TMR3G_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM2PR), base(8)) PWM2PR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM2), base(8)) PWM2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(INT1), base(8)) INT1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC2), base(8)) CLC2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CWG1), base(8)) CWG1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(NCO1), base(8)) NCO1_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA2SCNT), base(8)) DMA2SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA2DCNT), base(8)) DMA2DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA2OR), base(8)) DMA2OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA2A), base(8)) DMA2A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC3), base(8)) CLC3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM3PR), base(8)) PWM3PR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM3), base(8)) PWM3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U2RX), base(8)) U2RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U2TX), base(8)) U2TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U2E), base(8)) U2E_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U2), base(8)) U2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR5), base(8)) TMR5_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR5G), base(8)) TMR5G_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CCP2), base(8)) CCP2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(SCAN), base(8)) SCAN_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U3RX), base(8)) U3RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U3TX), base(8)) U3TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U3E), base(8)) U3E_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U3), base(8)) U3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC4), base(8)) CLC4_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM4PR), base(8)) PWM4PR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(PWM4), base(8)) PWM4_ISR() {
    __asm("RESET");
}
void __interrupt(irq(INT2), base(8)) INT2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC5), base(8)) CLC5_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CWG2), base(8)) CWG2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(NCO2), base(8)) NCO2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA3SCNT), base(8)) DMA3SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA3DCNT), base(8)) DMA3DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA3OR), base(8)) DMA3OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA3A), base(8)) DMA3A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CCP3), base(8)) CCP3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC6), base(8)) CLC6_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CWG3), base(8)) CWG3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR4), base(8)) TMR4_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA4SCNT), base(8)) DMA4SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA4DCNT), base(8)) DMA4DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA4OR), base(8)) DMA4OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA4A), base(8)) DMA4A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U4RX), base(8)) U4RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U4TX), base(8)) U4TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U4E), base(8)) U4E_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U4), base(8)) U4_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA5SCNT), base(8)) DMA5SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA5DCNT), base(8)) DMA5DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA5OR), base(8)) DMA5OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA5A), base(8)) DMA5A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U5RX), base(8)) U5RX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U5TX), base(8)) U5TX_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U5E), base(8)) U5E_ISR() {
    __asm("RESET");
}
void __interrupt(irq(U5), base(8)) U5_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA6SCNT), base(8)) DMA6SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA6DCNT), base(8)) DMA6DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA6OR), base(8)) DMA6OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA6A), base(8)) DMA6A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC7), base(8)) CLC7_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CM2), base(8)) CM2_ISR() {
    __asm("RESET");
}
void __interrupt(irq(NCO3), base(8)) NCO3_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA7SCNT), base(8)) DMA7SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA7DCNT), base(8)) DMA7DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA7OR), base(8)) DMA7OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA7A), base(8)) DMA7A_ISR() {
    __asm("RESET");
}
void __interrupt(irq(NVM), base(8)) NVM_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CLC8), base(8)) CLC8_ISR() {
    __asm("RESET");
}
void __interrupt(irq(CRC), base(8)) CRC_ISR() {
    __asm("RESET");
}
void __interrupt(irq(TMR6), base(8)) TMR6_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA8SCNT), base(8)) DMA8SCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA8DCNT), base(8)) DMA8DCNT_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA8OR), base(8)) DMA8OR_ISR() {
    __asm("RESET");
}
void __interrupt(irq(DMA8A), base(8)) DMA8A_ISR() {
    __asm("RESET");
}
