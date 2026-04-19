#include "adc.h"

#include "libcomm.h"

#include <xc.h>

/* Positive reference = FVR 2.048 V. The level-meter 0..190 Ω range at a 5 mA
 * current source sweeps 0..0.95 V, and 240..33 Ω sweeps 1.20..0.165 V — both
 * fit inside the 2.048 V reference with headroom. The 12 V battery is divided
 * by 10 to ~1.2 V nominal. DIA_FVRA2X calibration can be substituted for this
 * nominal value once `flash.c` is ported. */
#define ADC_FVR_MV 2048u

#define ADC_CHANNEL_WATER 0x05 /* RA5 */
#define ADC_CHANNEL_BATT 0x06  /* RA6 */
#define ADC_CHANNEL_FUEL 0x07  /* RA7 */

#define CH_WATER 0u
#define CH_BATT 1u
#define CH_FUEL 2u

/* EMA smoothing for level meters. A single burst (16-sample hardware average)
 * lands every ~1.5 ms while the sweep is free-running. SHIFT=12 gives a
 * first-order filter with ~4096-sample time constant ≈ 6 s, comfortably
 * spanning the 2..5 s roll/pitch period typical of an anchored boat in
 * moderate chop, so the indicated level reflects the mean float height
 * instead of each wave. */
#define LEVEL_EMA_SHIFT 12u

static const uint8_t channels[3] = {
    ADC_CHANNEL_WATER,
    ADC_CHANNEL_BATT,
    ADC_CHANNEL_FUEL,
};

static volatile uint16_t latest[3]; /* 12-bit burst-averaged reading */
static volatile uint32_t level_ema[2];
static volatile uint8_t sweep_idx;
static volatile uint8_t ema_primed[2];

static void start_burst(uint8_t idx);

void adc_init(void) {
    ANSELAbits.ANSELA5 = 1;
    ANSELAbits.ANSELA6 = 1;
    ANSELAbits.ANSELA7 = 1;
    TRISAbits.TRISA5 = 1;
    TRISAbits.TRISA6 = 1;
    TRISAbits.TRISA7 = 1;

    FVRCONbits.ADFVR = 0b10; /* 2.048 V */
    FVRCONbits.EN = 1;

    ADCON0bits.ON = 0;
    ADCON0bits.CONT = 0;
    ADCON0bits.FM = 1; /* right-justified */
    ADCON0bits.CS = 0; /* Fosc */

    ADCON1 = 0;
    ADCON2 = 0;
    ADCON2bits.CRS = 4;    /* /16 accumulator shift */
    ADCON2bits.MD = 0b011; /* burst-average */
    ADRPT = 16;
    ADCLK = 32; /* Fosc/64 -> 1 MHz Tad */

    ADREFbits.PREF = 0b11; /* Vref+ = FVR */
    ADPRE = 0;
    ADACQ = 16;

    latest[0] = latest[1] = latest[2] = 0;
    level_ema[0] = level_ema[1] = 0;
    ema_primed[0] = ema_primed[1] = 0;
    sweep_idx = 0;

    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;

    ADCON0bits.ON = 1;
    start_burst(sweep_idx);
}

void __interrupt(irq(AD), base(8)) AD_ISR(void) {
    PIR1bits.ADIF = 0;

    uint8_t idx = sweep_idx;
    uint16_t raw = (uint16_t)ADRES;
    latest[idx] = raw;

    if (idx == CH_WATER || idx == CH_FUEL) {
        uint8_t li = (idx == CH_WATER) ? 0u : 1u;
        uint32_t state = level_ema[li];
        if (!ema_primed[li]) {
            state = (uint32_t)raw << LEVEL_EMA_SHIFT;
            ema_primed[li] = 1;
        } else {
            state = state - (state >> LEVEL_EMA_SHIFT) + raw;
        }
        level_ema[li] = state;
    }

    idx = (uint8_t)((idx + 1u) % 3u);
    sweep_idx = idx;
    start_burst(idx);
}

uint16_t adc_read_battery_mv(void) {
    uint16_t raw;
    INTERRUPT_PUSH;
    raw = latest[CH_BATT];
    INTERRUPT_POP;
    /* V_pin = raw * FVR_mV / 4096; V_batt = V_pin * 10. */
    return (uint16_t)((uint32_t)raw * ADC_FVR_MV * 10u / 4096u);
}

uint8_t adc_read_level_fresh_water(void) {
    uint32_t s;
    INTERRUPT_PUSH;
    s = level_ema[0];
    INTERRUPT_POP;
    return (uint8_t)((s >> LEVEL_EMA_SHIFT) >> 4);
}

uint8_t adc_read_level_fuel(void) {
    uint32_t s;
    INTERRUPT_PUSH;
    s = level_ema[1];
    INTERRUPT_POP;
    return (uint8_t)((s >> LEVEL_EMA_SHIFT) >> 4);
}

static void start_burst(uint8_t idx) {
    ADCON2bits.ACLR = 1;
    ADPCH = channels[idx];
    ADCON0bits.GO = 1;
}
