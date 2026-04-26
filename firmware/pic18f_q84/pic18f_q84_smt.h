/*
 * pic18f_q84_smt.h
 *
 * SMT (Signal Measurement Timer) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The SMT is a 24-bit timer/counter with hardware capture registers,
 * designed for high-resolution signal measurement.  It can measure
 * periods, pulse widths, duty cycles, frequencies, and time-of-flight
 * intervals entirely in hardware, without software intervention during
 * the measurement.
 *
 * Key features:
 *   - 24-bit free-running timer with prescaler (1:1 to 1:8)
 *   - 24-bit period register (SMT1PR) for match/rollover
 *   - Two 24-bit capture registers:
 *       CPR (Capture Period Register) - captures period or timestamps
 *       CPW (Capture Pulse Width Register) - captures pulse widths
 *   - Nine operating modes:
 *       Timer, Windowed counter, Gated counter, Period & duty cycle,
 *       High & low time, Windowed measurement, Gated windowed
 *       measurement, Time-of-flight, Capture
 *   - Independent clock, signal, and window source selection
 *   - Single-shot or auto-repeat acquisition
 *   - Status flags for capture-ready, acquisition, and timer state
 *
 * The SMT1 instance occupies addresses 0x0300-0x0311.
 *
 * Note: 24-bit registers are accessed as three individual bytes
 * (Low, High, Upper) since the PIC18 data bus is 8 bits wide.
 * Hardware double-buffering ensures coherent reads/writes across
 * the three bytes when accessed through the TMR, CPR, CPW, and PR
 * register sets.
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_SMT_H
#define PIC18F_Q84_SMT_H

/* ===================================================================
 * SMT1TMR - SMT1 Timer Register (24-bit, read/write)
 * Addresses: 0x0300 (Low), 0x0301 (High), 0x0302 (Upper)
 *
 * The 24-bit free-running counter value.  In timer mode the counter
 * increments on each prescaled clock edge and resets to zero when
 * it matches SMT1PR.  In measurement modes the counter runs during
 * acquisition and its value is captured into CPR and/or CPW by
 * hardware signal edges.
 *
 * The timer register can be written by software to preset a starting
 * value.  Writes should be performed while the timer is stopped
 * (GO = 0 or STP = 1) to avoid race conditions.
 *
 * Read order: read TMRL first, then TMRH, then TMRU.  The hardware
 * latches the upper bytes when TMRL is read, ensuring a coherent
 * 24-bit snapshot.
 * ================================================================ */
#define SMT1TMRL SFR8(0x0300) /* Bits  7:0  of counter   */
#define SMT1TMRH SFR8(0x0301) /* Bits 15:8  of counter   */
#define SMT1TMRU SFR8(0x0302) /* Bits 23:16 of counter   */

/* 16-bit access to the lower two bytes (bits 15:0)                  */
#define SMT1TMR SFR16(0x0300)

/* ===================================================================
 * SMT1CPR - SMT1 Capture Period Register (24-bit, read-only)
 * Addresses: 0x0303 (Low), 0x0304 (High), 0x0305 (Upper)
 *
 * Hardware-captured timer value, loaded automatically on specific
 * signal events depending on the operating mode:
 *
 *   Period & duty cycle mode : captures the full period (rising to
 *                              rising edge of the signal input)
 *   High & low time mode    : captures the low-time duration
 *   Windowed measurement    : captures the timer value on the
 *                              window edge
 *   Time-of-flight mode     : captures the elapsed time between
 *                              signal and window edges
 *   Capture mode            : captures the timer on signal edge
 *
 * The CPRUP bit in SMT1STAT is set when new data is loaded into
 * this register, and cleared by hardware when the register is read
 * or a new acquisition begins.
 * ================================================================ */
#define SMT1CPRL SFR8(0x0303) /* Bits  7:0  of CPR       */
#define SMT1CPRH SFR8(0x0304) /* Bits 15:8  of CPR       */
#define SMT1CPRU SFR8(0x0305) /* Bits 23:16 of CPR       */

/* 16-bit access to the lower two bytes (bits 15:0)                  */
#define SMT1CPR SFR16(0x0303)

/* ===================================================================
 * SMT1CPW - SMT1 Capture Pulse Width Register (24-bit, read-only)
 * Addresses: 0x0306 (Low), 0x0307 (High), 0x0308 (Upper)
 *
 * Hardware-captured timer value for pulse width measurements:
 *
 *   Period & duty cycle mode : captures the high-time (pulse width)
 *                              of the signal input
 *   High & low time mode    : captures the high-time duration
 *
 * The CPWUP bit in SMT1STAT is set when new data is loaded, and
 * cleared by hardware when read or when a new acquisition begins.
 * ================================================================ */
#define SMT1CPWL SFR8(0x0306) /* Bits  7:0  of CPW       */
#define SMT1CPWH SFR8(0x0307) /* Bits 15:8  of CPW       */
#define SMT1CPWU SFR8(0x0308) /* Bits 23:16 of CPW       */

/* 16-bit access to the lower two bytes (bits 15:0)                  */
#define SMT1CPW SFR16(0x0306)

/* ===================================================================
 * SMT1PR - SMT1 Period Register (24-bit, read/write)
 * Addresses: 0x0309 (Low), 0x030A (High), 0x030B (Upper)
 *
 * The period match value.  In timer mode (MODE = 0000), the counter
 * resets to zero and generates a period-match event when SMT1TMR
 * equals SMT1PR.  In other modes this register may be unused or
 * serve a mode-specific function.
 *
 * Write all three bytes (PRL, PRH, PRU) while the timer is stopped
 * to set the desired period.  The maximum period is 0xFFFFFF
 * (16,777,215) counts.
 * ================================================================ */
#define SMT1PRL SFR8(0x0309) /* Bits  7:0  of period    */
#define SMT1PRH SFR8(0x030A) /* Bits 15:8  of period    */
#define SMT1PRU SFR8(0x030B) /* Bits 23:16 of period    */

/* 16-bit access to the lower two bytes (bits 15:0)                  */
#define SMT1PR SFR16(0x0309)

/* ===================================================================
 * SMT1CON0 - SMT1 Control Register 0
 * Address: 0x030C
 *
 * Controls the enable state, stop function, signal/window polarity,
 * and prescaler division ratio of the SMT1 module.
 *
 * Prescaler options allow trading time resolution for measurement
 * range: at 1:8 prescale with a 64 MHz clock, the timer can measure
 * intervals up to ~2.1 seconds with 125 ns resolution.
 * ================================================================ */
#define SMT1CON0 SFR8(0x030C)

/* Bit 7 - EN: SMT1 Enable
 *   1 = SMT1 module is enabled; clock, signal, and window inputs
 *       are active and the timer is ready to count (pending GO).
 *   0 = SMT1 module is disabled; all internal logic is held in
 *       reset and the module draws minimal current.
 *   Clearing EN resets the timer and all status bits.               */
#define SMT1CON0_EN 7

/* Bit 6 - Reserved: read as 0, write as 0.                         */

/* Bit 5 - STP: Stop
 *   Write 1 = Halt the timer immediately.  The current count value
 *             is preserved in SMT1TMR.  The GO bit in SMT1CON1 is
 *             cleared by hardware.
 *   Write 0 = No effect (the timer does not restart; use GO to
 *             restart).
 *   Read  0 = Always reads as 0 (write-only, self-clearing).
 *
 *   STP provides a means to abort a measurement in progress without
 *   disabling the entire module.                                    */
#define SMT1CON0_STP 5

/* Bit 4 - Reserved: read as 0, write as 0.                         */

/* Bit 3 - WPOL: Window Input Polarity
 *   1 = Window input is active-high (window open when pin is high).
 *   0 = Window input is active-low (window open when pin is low).
 *
 *   Applies to modes that use a window signal: windowed counter,
 *   windowed measurement, gated windowed measurement, and
 *   time-of-flight.                                                 */
#define SMT1CON0_WPOL 3

/* Bit 2 - SPOL: Signal Input Polarity
 *   1 = Signal input is active-high / rising-edge sensitive.
 *   0 = Signal input is active-low / falling-edge sensitive.
 *
 *   Determines which edge or level of the signal input triggers
 *   capture events and counter gating, depending on the selected
 *   operating mode.                                                 */
#define SMT1CON0_SPOL 2

/* Bits 1:0 - PS[1:0]: Prescaler Select
 *   Divides the clock source before it drives the 24-bit counter:
 *   00 = 1:1 (no prescaling; full clock rate)
 *   01 = 1:2
 *   10 = 1:4
 *   11 = 1:8
 *
 *   Higher prescale ratios extend the maximum measurable interval
 *   at the cost of reduced timing resolution.                       */
#define SMT1CON0_PS1 1
#define SMT1CON0_PS0 0

/* PS field mask and position                                        */
#define SMT1CON0_PS_MASK 0x03 /* bits 1:0 */
#define SMT1CON0_PS_POS 0

/* Named prescaler values for PS[1:0]                                */
#define SMT1PS_1 0x00 /* 1:1 (no prescaling)             */
#define SMT1PS_2 0x01 /* 1:2                             */
#define SMT1PS_4 0x02 /* 1:4                             */
#define SMT1PS_8 0x03 /* 1:8                             */

/* ===================================================================
 * SMT1CON1 - SMT1 Control Register 1
 * Address: 0x030D
 *
 * Controls the timer start/status, repeat-acquisition behaviour,
 * and the operating mode of the SMT1 module.
 *
 * Operating modes:
 *
 *   Timer (0000):
 *     Free-running counter that increments on each prescaled clock
 *     edge.  Resets to zero on a match with SMT1PR and sets the
 *     period-match interrupt flag.
 *
 *   Windowed counter (0001):
 *     Counts signal edges that occur while the window input is
 *     active.  The count is captured into CPR when the window
 *     closes.
 *
 *   Gated counter (0010):
 *     Counts prescaled clock edges while the signal input is active.
 *     The count is captured when the signal deasserts.
 *
 *   Period and duty cycle (0011):
 *     Measures the period (rising-to-rising) in CPR and the pulse
 *     width (rising-to-falling) in CPW of the signal input.
 *
 *   High and low time (0100):
 *     Measures the high time in CPW and the low time in CPR of the
 *     signal input.
 *
 *   Windowed measurement (0101):
 *     Captures the timer value into CPR on the window edge.
 *
 *   Gated windowed measurement (0110):
 *     Captures the timer value into CPR on the window edge, but
 *     only while the signal input is active.
 *
 *   Time-of-flight (0111):
 *     Measures the elapsed time between the signal edge and the
 *     window edge.  Timer starts on the signal edge and captures
 *     into CPR on the window edge.
 *
 *   Capture (1000):
 *     Captures the free-running timer value into CPR on each signal
 *     edge.  The timer runs continuously; successive captures can
 *     be subtracted in software to compute intervals.
 * ================================================================ */
#define SMT1CON1 SFR8(0x030D)

/* Bit 7 - GO: Timer Start / Status
 *   Write 1 = Start the timer and begin acquisition (in measurement
 *             modes) or start counting (in timer mode).
 *   Write 0 = No effect (use STP to stop the timer).
 *   Read  1 = Timer is actively running or acquisition is in
 *             progress.
 *   Read  0 = Timer is stopped (either not started, completed
 *             single-shot acquisition, or halted by STP).
 *
 *   In single-shot modes (REPEAT = 0), hardware clears GO
 *   automatically when the acquisition completes.                   */
#define SMT1CON1_GO 7

/* Bit 6 - REPEAT: Repeat Acquisition Enable
 *   1 = Auto-repeat mode.  After each acquisition completes, the
 *       timer automatically restarts a new acquisition cycle.
 *       Captured data is updated continuously.
 *   0 = Single-shot mode.  One acquisition is performed, then the
 *       timer stops and GO is cleared by hardware.  Software must
 *       set GO again to start a new measurement.                    */
#define SMT1CON1_REPEAT 6

/* Bits 5:4 - Reserved: read as 0, write as 0.                      */

/* Bits 3:0 - MODE[3:0]: Operating Mode Select
 *   See register description above for mode assignments.            */
#define SMT1CON1_MODE3 3
#define SMT1CON1_MODE2 2
#define SMT1CON1_MODE1 1
#define SMT1CON1_MODE0 0

/* MODE field mask and position                                      */
#define SMT1CON1_MODE_MASK 0x0F /* bits 3:0 */
#define SMT1CON1_MODE_POS 0

/* Named mode values for MODE[3:0]                                   */
#define SMT1MODE_TIMER 0x00     /* Timer (free-running, PR match)  */
#define SMT1MODE_WCOUNT 0x01    /* Windowed counter                */
#define SMT1MODE_GCOUNT 0x02    /* Gated counter                   */
#define SMT1MODE_PERIOD_DC 0x03 /* Period and duty cycle            */
#define SMT1MODE_HILO 0x04      /* High and low time               */
#define SMT1MODE_WMEAS 0x05     /* Windowed measurement            */
#define SMT1MODE_GWMEAS 0x06    /* Gated windowed measurement      */
#define SMT1MODE_TOF 0x07       /* Time-of-flight                  */
#define SMT1MODE_CAPTURE 0x08   /* Capture                         */

/* ===================================================================
 * SMT1STAT - SMT1 Status Register
 * Address: 0x030E
 *
 * Provides status flags for the SMT1 module including capture-ready
 * indicators, a software timer reset, and read-only status bits for
 * the timer, window, and acquisition states.
 * ================================================================ */
#define SMT1STAT SFR8(0x030E)

/* Bit 7 - CPRUP: Capture Period Register Updated (read-only)
 *   1 = SMT1CPR contains new data from the most recent capture
 *       event.  Cleared by hardware when SMT1CPRL is read or
 *       when a new acquisition begins.
 *   0 = No new data in SMT1CPR since last read.
 *
 *   Poll this bit to determine when a period or timestamp
 *   measurement is available for software processing.               */
#define SMT1STAT_CPRUP 7

/* Bit 6 - CPWUP: Capture Pulse Width Register Updated (read-only)
 *   1 = SMT1CPW contains new data from the most recent capture.
 *       Cleared by hardware when SMT1CPWL is read or when a new
 *       acquisition begins.
 *   0 = No new data in SMT1CPW since last read.
 *
 *   Poll this bit to determine when a pulse width measurement
 *   is available.                                                   */
#define SMT1STAT_CPWUP 6

/* Bit 5 - RST: Timer Reset (write-only)
 *   Write 1 = Reset the 24-bit timer counter (SMT1TMR) to zero.
 *             The GO bit is not affected; the timer continues
 *             running if it was already started.
 *   Write 0 = No effect.
 *   Read  0 = Always reads as 0 (self-clearing).
 *
 *   Useful for synchronizing the timer to an external event by
 *   resetting the count at a known instant.                         */
#define SMT1STAT_RST 5

/* Bits 4:3 - Reserved: read as 0, write as 0.                      */

/* Bit 2 - TS: Timer Status (read-only)
 *   1 = Timer is actively counting (prescaled clock edges are
 *       incrementing SMT1TMR).
 *   0 = Timer is not counting (stopped, disabled, or awaiting a
 *       gate/window signal depending on the mode).                  */
#define SMT1STAT_TS 2

/* Bit 1 - WS: Window Status (read-only)
 *   1 = Window input is currently in the active state (after
 *       polarity inversion via WPOL).
 *   0 = Window input is inactive.
 *
 *   Meaningful only in windowed modes (windowed counter, windowed
 *   measurement, gated windowed measurement, time-of-flight).      */
#define SMT1STAT_WS 1

/* Bit 0 - AS: Acquisition Status (read-only)
 *   1 = An acquisition is currently in progress.  The module is
 *       waiting for or actively measuring a signal event.
 *   0 = No acquisition in progress.  Either the module has not
 *       been started, or a single-shot acquisition has completed.   */
#define SMT1STAT_AS 0

/* ===================================================================
 * SMT1CLK - SMT1 Clock Source Select Register
 * Address: 0x030F
 *
 * Selects the clock source that drives the 24-bit timer/counter
 * through the prescaler.  The clock source determines the timing
 * resolution of all measurements.
 *
 * The specific mapping of CSEL values to clock signals is defined
 * in the "SMT Clock Source Selection" table of the data sheet.
 * Common sources include FOSC, HFINTOSC, MFINTOSC, LFINTOSC,
 * SOSC, and CLKREF output.
 * ================================================================ */
#define SMT1CLK SFR8(0x030F)

/* Bits 7:4 - Reserved: read as 0, write as 0.                      */

/* Bits 3:0 - CSEL[3:0]: Clock Source Select
 *   Selects the clock input to the SMT prescaler.
 *   Refer to DS40002213D Table "SMT Clock Source Selection" for
 *   the full mapping of source codes to clock signals.              */
#define SMT1CLK_CSEL3 3
#define SMT1CLK_CSEL2 2
#define SMT1CLK_CSEL1 1
#define SMT1CLK_CSEL0 0

/* CSEL field mask and position                                      */
#define SMT1CLK_CSEL_MASK 0x0F /* bits 3:0 */
#define SMT1CLK_CSEL_POS 0

/* ===================================================================
 * SMT1SIG - SMT1 Signal Source Select Register
 * Address: 0x0310
 *
 * Selects the signal input for the SMT1 module.  The signal is the
 * primary input whose edges or levels trigger captures, gating, or
 * counting depending on the operating mode.  The signal polarity
 * is controlled by SPOL in SMT1CON0.
 *
 * The specific mapping of SSEL values to signal sources is defined
 * in the "SMT Signal Source Selection" table of the data sheet.
 * Sources include CLC outputs, comparator outputs, IOC pin events,
 * timer overflows, and other peripheral signals.
 * ================================================================ */
#define SMT1SIG SFR8(0x0310)

/* Bits 7:6 - Reserved: read as 0, write as 0.                      */

/* Bits 5:0 - SSEL[5:0]: Signal Input Source Select
 *   Selects the signal input to the SMT module.
 *   Refer to DS40002213D Table "SMT Signal Source Selection" for
 *   the full mapping.                                               */
#define SMT1SIG_SSEL5 5
#define SMT1SIG_SSEL4 4
#define SMT1SIG_SSEL3 3
#define SMT1SIG_SSEL2 2
#define SMT1SIG_SSEL1 1
#define SMT1SIG_SSEL0 0

/* SSEL field mask and position                                      */
#define SMT1SIG_SSEL_MASK 0x3F /* bits 5:0 */
#define SMT1SIG_SSEL_POS 0

/* ===================================================================
 * SMT1WIN - SMT1 Window Source Select Register
 * Address: 0x0311
 *
 * Selects the window input for the SMT1 module.  The window signal
 * defines a gating interval during which the timer counts or
 * captures occur, depending on the operating mode.  The window
 * polarity is controlled by WPOL in SMT1CON0.
 *
 * The window input is used by: windowed counter, windowed
 * measurement, gated windowed measurement, and time-of-flight
 * modes.  In other modes the window input is ignored.
 *
 * The specific mapping of WSEL values to window sources is defined
 * in the "SMT Window Source Selection" table of the data sheet.
 * Sources include CLC outputs, comparator outputs, IOC pin events,
 * timer overflows, and other peripheral signals.
 * ================================================================ */
#define SMT1WIN SFR8(0x0311)

/* Bits 7:6 - Reserved: read as 0, write as 0.                      */

/* Bits 5:0 - WSEL[5:0]: Window Input Source Select
 *   Selects the window input to the SMT module.
 *   Refer to DS40002213D Table "SMT Window Source Selection" for
 *   the full mapping.                                               */
#define SMT1WIN_WSEL5 5
#define SMT1WIN_WSEL4 4
#define SMT1WIN_WSEL3 3
#define SMT1WIN_WSEL2 2
#define SMT1WIN_WSEL1 1
#define SMT1WIN_WSEL0 0

/* WSEL field mask and position                                      */
#define SMT1WIN_WSEL_MASK 0x3F /* bits 5:0 */
#define SMT1WIN_WSEL_POS 0

#endif /* PIC18F_Q84_SMT_H */
