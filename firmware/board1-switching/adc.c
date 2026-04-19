#include "adc.h"

#include <xc.h>

/* Positive reference = FVR 2.048 V. The level-meter 0..190 Ω range at a 5 mA
 * current source sweeps 0..0.95 V, and 240..33 Ω sweeps 1.20..0.165 V — both
 * fit inside the 2.048 V reference with headroom. The 12 V battery is divided
 * by 10 to ~1.2 V nominal. DIA_FVRA2X calibration can be substituted for this
 * nominal value once `flash.c` is ported. */
#define ADC_FVR_MV 2048u

#define ADC_CHANNEL_RA5 0x05
#define ADC_CHANNEL_RA6 0x06
#define ADC_CHANNEL_RA7 0x07

static uint16_t sample_burst(uint8_t channel);

void adc_init(void) {
    /* Fresh water (RA5), battery (RA6), fuel (RA7) as analog inputs. */
    ANSELAbits.ANSELA5 = 1;
    ANSELAbits.ANSELA6 = 1;
    ANSELAbits.ANSELA7 = 1;
    TRISAbits.TRISA5 = 1;
    TRISAbits.TRISA6 = 1;
    TRISAbits.TRISA7 = 1;

    FVRCONbits.ADFVR = 0b10; // 2.048 V
    FVRCONbits.EN = 1;

    ADCON0bits.ON = 0;
    ADCON0bits.CONT = 0;
    ADCON0bits.FM = 1; // right-justified result
    ADCON0bits.CS = 0; // Fosc

    ADCON1 = 0;
    ADCON2 = 0;
    ADCON2bits.CRS = 4;    // accumulator shift /16 => average
    ADCON2bits.MD = 0b011; // burst-average mode
    ADRPT = 16;
    ADCLK = 32; // Fosc/64 -> 1 MHz Tad

    ADREFbits.PREF = 0b11; // Vref+ = FVR
    ADPRE = 0;
    ADACQ = 16; // 16 Tad acquisition = 16 µs

    ADCON0bits.ON = 1;
}

static uint16_t sample_burst(uint8_t channel) {
    ADCON2bits.ACLR = 1; // clear ADACC / ADCNT
    ADPCH = channel;
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO) {} // GO self-clears at burst completion
    return (uint16_t)ADRES;
}

uint16_t adc_read_battery_mv(void) {
    /* V_pin = raw * FVR_mV / 4096; V_batt = V_pin * 10. */
    uint16_t raw = sample_burst(ADC_CHANNEL_RA6);
    uint32_t v = (uint32_t)raw * ADC_FVR_MV * 10u / 4096u;
    return (uint16_t)v;
}

uint8_t adc_read_level_fresh_water(void) {
    return (uint8_t)(sample_burst(ADC_CHANNEL_RA5) >> 4);
}

uint8_t adc_read_level_fuel(void) {
    return (uint8_t)(sample_burst(ADC_CHANNEL_RA7) >> 4);
}
