#include "adc.h"

#include "config.h"
#include "controller.h"
#include "task.h"

#include <xc.h>
#include "libcomm.h"

/* Positive reference = FVR 2.048 V. The level-meter 0..190 Ω range at a 5 mA
 * current source sweeps 0..0.95 V, and 240..33 Ω sweeps 1.20..0.165 V — both
 * fit inside the 2.048 V reference with headroom. The 12 V battery is divided
 * by 10 to ~1.2 V nominal. DIA_FVRA2X carries the chip's factory FVR-at-2x
 * calibration in mV (read from program flash on boot). */

#define ADC_CHANNEL_WATER 0x05 /* RA5 */
#define ADC_CHANNEL_BATT 0x06  /* RA6 */
#define ADC_CHANNEL_FUEL 0x07  /* RA7 */

#define CH_WATER 0u
#define CH_BATT 1u
#define CH_FUEL 2u

/* Nominal full-scale ADC mV for the level meters — 5 mA × 190 Ω = 950 mV.
 * level_raw_to_ohm uses this as the denominator that maps ADC mV → Ω
 * across the 0..190 Ω scale. The battery channel multiplies by 10
 * directly (the divider ratio), so it doesn't need a constant here. */
#define NOMINAL_ADC_MV_LEVEL 950u

/* Resistance span for each meter mode — used to map calibrated Ω → 0..100%. */
#define MODE_240_33_EMPTY_OHM 240u
#define MODE_240_33_FULL_OHM 33u
#define MODE_0_190_EMPTY_OHM 0u
#define MODE_0_190_FULL_OHM 190u

/* Over-range sentinel: when the calibrated Ω exceeds 1.5× the channel's
 * normal max, the float is almost certainly open / disconnected and the
 * current source rails. Report 255 (above the legal 0..100 percent range)
 * so the main board's UI can show "ERR" rather than a misleading 0 % or
 * 100 % reading. */
#define MODE_240_33_OVER_RANGE_OHM 360u /* 1.5 × MODE_240_33_EMPTY_OHM */
#define MODE_0_190_OVER_RANGE_OHM 285u  /* 1.5 × MODE_0_190_FULL_OHM   */
#define LEVEL_OVER_RANGE_SENTINEL 255u

static const uint8_t channels[3] = {
    ADC_CHANNEL_WATER,
    ADC_CHANNEL_BATT,
    ADC_CHANNEL_FUEL,
};

static volatile uint16_t latest_raw[3]; /* raw 12-bit burst-averaged count */
static volatile uint8_t sweep_idx;

/* Processed values — written from main-loop processors, read from any
 * context (incl. the comm sync handler in ISR). INTERRUPT_PUSH guards the
 * 16-bit battery read against torn updates. */
static volatile uint16_t battery_real_mv;
static volatile uint8_t water_pct;
static volatile uint8_t fuel_pct;

/* Sloshing compensation — first-order IIR (running average) on the
 * calibrated Ω value, applied before the mode → percent mapping so a
 * mode switch shows up immediately while wave-induced float oscillation
 * is filtered out. Each level processor runs every ~3 ms (the sweep
 * visits its channel once per round), so SHIFT=11 gives a time constant
 * of ~2048 samples × 3 ms ≈ 6 s — well above the 2..5 s roll/pitch
 * period of an anchored boat in moderate chop.
 *
 * Battery uses a much shorter τ (~400 ms) so transient inverter or
 * starter loads get smoothed but actual battery sag is still visible. */
#define LEVEL_EMA_SHIFT 11u
#define BATTERY_EMA_SHIFT 7u

static uint32_t water_ema_state;
static uint32_t fuel_ema_state;
static uint32_t battery_ema_state;
static uint8_t water_ema_primed;
static uint8_t fuel_ema_primed;
static uint8_t battery_ema_primed;

static uint16_t g_max_voltage_mv;
static TaskController* g_ctrl;

static void start_burst(uint8_t idx);
static uint16_t flash_read_word(uint32_t addr);
static void process_water(void* ctx);
static void process_battery(void* ctx);
static void process_fuel(void* ctx);

void adc_init(TaskController* ctrl) {
    g_ctrl = ctrl;
    g_max_voltage_mv = flash_read_word(DIA_FVRA2X);

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

    ADCON3bits.CALC = 0b001; // ADRES-ADSTPT. Actual result vs. setpoint
    ADSTATbits.MATH = 0;     // ADMATH registers not updated(0)

    ADREFbits.PREF = 0b11; /* Vref+ = FVR */
    ADPRE = 0;
    /* Acquisition time per sample, in Tad (1 us). High-impedance level
     * sensors need the S/H cap a chance to settle between burst samples;
     * 50 us is comfortable and still <1 ms per 16-sample burst. */
    ADACQ = 50;

    latest_raw[0] = latest_raw[1] = latest_raw[2] = 0;
    sweep_idx = 0;
    battery_real_mv = 0;
    water_pct = 0;
    fuel_pct = 0;
    water_ema_state = 0;
    fuel_ema_state = 0;
    battery_ema_state = 0;
    water_ema_primed = 0;
    fuel_ema_primed = 0;
    battery_ema_primed = 0;

    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;

    ADCON0bits.ON = 1;
    start_burst(sweep_idx);
}

/* AD completion ISR — keeps the bus moving and hands the conversion result
 * to main context. Three things only: latch the raw count, advance the
 * channel, kick off the next burst. The per-channel converter runs on the
 * main loop via run_in_main_loop so config/EEPROM reads and the
 * normalisation arithmetic don't sit inside the ISR. */
void __interrupt(low_priority, irq(AD), base(8)) AD_ISR(void) {
    PIR1bits.ADIF = 0;
    if (ADCNT != ADRPT) {
        return;
    }

    uint8_t idx = sweep_idx;
    latest_raw[idx] = (uint16_t)ADRES;

    switch (idx) {
        case CH_WATER: run_in_main_loop(g_ctrl, process_water,   0); break;
        case CH_BATT:  run_in_main_loop(g_ctrl, process_battery, 0); break;
        case CH_FUEL:  run_in_main_loop(g_ctrl, process_fuel,    0); break;
    }

    idx = (uint8_t)((idx + 1u) % 3u);
    sweep_idx = idx;

    ADCON2bits.ACLR = 1;
    ADPCH = channels[idx];
    ADCON0bits.GO = 1;
}

/* ============================================================================
 * Main-loop processors — one per channel.
 *
 * Pipeline (shared shape):
 *   1. Snapshot the raw ADC count under INTERRUPT_PUSH.
 *   2. Apply the per-channel signed trim from config (CONFIG_ADDR_*_OFFSET /
 *      CONFIG_ADDR_BATTERY_CAL), clamped into the 12-bit ADC range.
 *   3. Convert raw → ADC mV via the FVR DIA calibration.
 *   4. Normalise into real units (mV for battery, Ω for levels) against the
 *      (minimal, nominal) calibration pair stored in EEPROM.
 *   5. Levels only: map Ω → 0..100% via the configured CommMeterMode.
 *   6. Publish to the volatile shadow read by adc_read_*.
 * ============================================================================
 */

static uint16_t raw_to_mv(uint16_t raw) {
    return (uint16_t)(((uint32_t)raw * g_max_voltage_mv) / 0x0FFFu);
}

static uint8_t ohm_to_percent(uint16_t ohm, uint8_t mode) {
    switch (mode) {
        case COMM_METER_MODE_240_33:
            if (ohm > MODE_240_33_OVER_RANGE_OHM) {
                return LEVEL_OVER_RANGE_SENTINEL;
            }
            if (ohm >= MODE_240_33_EMPTY_OHM) {
                return 0;
            }
            if (ohm <= MODE_240_33_FULL_OHM) {
                return 100;
            }
            return (uint8_t)(((uint32_t)(MODE_240_33_EMPTY_OHM - ohm) * 100u) /
                             (MODE_240_33_EMPTY_OHM - MODE_240_33_FULL_OHM));
        case COMM_METER_MODE_0_190:
            if (ohm > MODE_0_190_OVER_RANGE_OHM) {
                return LEVEL_OVER_RANGE_SENTINEL;
            }
            if (ohm >= MODE_0_190_FULL_OHM) {
                return 100;
            }
            return (uint8_t)(((uint32_t)ohm * 100u) / MODE_0_190_FULL_OHM);
        default:
            /* Mode 0 = COMM_METER_MODE_CALIBRATION — passthrough the calibrated
             * Ω value (clamped to the 0..255 byte range) so the user can
             * observe the float's actual resistance while picking the
             * scale-at-100Ω calibration byte. */
            return (ohm > 255u) ? 255u : (uint8_t)ohm;
    }
}

/* First-order EMA step. Seeds the state on first call to avoid the long
 * settling ramp from zero. `shift` is the right-shift used as both the
 * fixed-point scale and the time-constant exponent (τ ≈ 2^shift samples). */
static uint16_t ema_step(uint32_t* state, uint8_t* primed, uint16_t value, uint8_t shift) {
    uint32_t s = *state;
    if (!*primed) {
        s = (uint32_t)value << shift;
        *primed = 1;
    } else {
        s = s - (s >> shift) + value;
    }
    *state = s;
    return (uint16_t)(s >> shift);
}

static void process_battery(void* ctx) {
    (void)ctx;
    uint16_t raw;
    INTERRUPT_PUSH;
    raw = latest_raw[CH_BATT];
    INTERRUPT_POP;
    uint16_t adc_mv = raw_to_mv(raw);
    /* Standard /10 divider: real mV = adc_mv × 10. Capped to fit uint16. */
    uint32_t real_mv32 = (uint32_t)adc_mv * 10u;
    uint16_t real_mv = (real_mv32 > 0xFFFFu) ? 0xFFFFu : (uint16_t)real_mv32;
    /* Scale-at-12000mV calibration: cal byte is the displayed value in
     * 100 mV units when 12000 mV was applied. Default 120 (= 12000/100)
     * is no-op; cal=0 falls back to default to avoid divide-by-zero. */
    uint8_t cal = config_read_byte(CONFIG_ADDR_BATTERY_CAL);
    if (cal == 0) {
        cal = 120;
    }
    uint32_t corrected = (uint32_t)real_mv * 120u / cal;
    if (corrected > 0xFFFFu) {
        corrected = 0xFFFFu;
    }
    uint16_t smoothed = ema_step(&battery_ema_state, &battery_ema_primed,
                                 (uint16_t)corrected, BATTERY_EMA_SHIFT);
    INTERRUPT_PUSH_NDECL;
    battery_real_mv = smoothed;
    INTERRUPT_POP;
}

/* Convert raw burst-averaged ADC count → calibrated Ω, with the
 * scale-at-100Ω correction applied. cal_addr stores the byte the user
 * read (in mode-0 passthrough) when 100 Ω was applied to this channel,
 * so corrected = raw * 100 / cal. cal=100 (default) is a no-op; cal=0
 * is treated as "uncalibrated, use default" to avoid divide-by-zero. */
static uint16_t level_raw_to_ohm(uint16_t raw, uint8_t cal_addr) {
    uint16_t adc_mv = raw_to_mv(raw);
    /* Ohm's law at the 5 mA constant current source: Ω = adc_mv / 5.
     * Equivalent to adc_mv * 190 / 950 across the full 0..190 Ω scale. */
    uint16_t raw_ohm = (uint16_t)((uint32_t)adc_mv * 190u / 950u);
    uint8_t cal = config_read_byte(cal_addr);
    if (cal == 0) {
        cal = 100;
    }
    uint32_t corrected = (uint32_t)raw_ohm * 100u / cal;
    return (corrected > 0xFFFFu) ? 0xFFFFu : (uint16_t)corrected;
}

static void process_water(void* ctx) {
    (void)ctx;
    uint16_t raw;
    INTERRUPT_PUSH;
    raw = latest_raw[CH_WATER];
    INTERRUPT_POP;
    uint16_t ohm = level_raw_to_ohm(raw, CONFIG_ADDR_WATER_CAL);
    uint16_t smoothed = ema_step(&water_ema_state, &water_ema_primed, ohm, LEVEL_EMA_SHIFT);
    /* Meter 0 mode = low 2 bits of the packed CommLevelMode byte. */
    water_pct = ohm_to_percent(smoothed, (uint8_t)(controller_level_mode() & 0x03));
}

static void process_fuel(void* ctx) {
    (void)ctx;
    uint16_t raw;
    INTERRUPT_PUSH;
    raw = latest_raw[CH_FUEL];
    INTERRUPT_POP;
    uint16_t ohm = level_raw_to_ohm(raw, CONFIG_ADDR_FUEL_CAL);
    uint16_t smoothed = ema_step(&fuel_ema_state, &fuel_ema_primed, ohm, LEVEL_EMA_SHIFT);
    /* Meter 1 mode = bits [3:2] of the packed CommLevelMode byte. */
    fuel_pct = ohm_to_percent(smoothed, (uint8_t)((controller_level_mode() >> 2) & 0x03));
}

/* ============================================================================
 * Read accessors
 * ============================================================================
 */

uint16_t adc_read_battery_mv(void) {
    uint16_t v;
    INTERRUPT_PUSH;
    v = battery_real_mv;
    INTERRUPT_POP;
    return v;
}

uint8_t adc_read_level_fresh_water(void) {
    return water_pct;
}

uint8_t adc_read_level_fuel(void) {
    return fuel_pct;
}

static void start_burst(uint8_t idx) {
    ADCON2bits.ACLR = 1;
    ADPCH = channels[idx];
    ADCON0bits.GO = 1;
}

static uint16_t flash_read_word(uint32_t addr) {
    uint8_t result_l, result_h;
    TBLPTRU = (uint8_t)((addr & 0x00FF0000) >> 16);
    TBLPTRH = (uint8_t)((addr & 0x0000FF00) >> 8);
    TBLPTRL = (uint8_t)(addr & 0x000000FF);
    asm("TBLRD*+");
    result_l = TABLAT;
    asm("TBLRD");
    result_h = TABLAT;

    return (((uint16_t)result_h << 8) | (result_l));
}
