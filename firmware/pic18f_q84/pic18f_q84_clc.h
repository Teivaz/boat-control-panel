/*
 * pic18f_q84_clc.h
 *
 * CLC (Configurable Logic Cell) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The CLC module provides eight independent Configurable Logic Cells
 * (CLC1-CLC8), each implementing a programmable combinational or
 * sequential logic function with four data inputs.  Available logic
 * functions include:
 *
 *   - AND-OR           : four AND gates feeding a single OR gate
 *   - OR-XOR           : four OR gates feeding an XOR tree
 *   - 4-input AND      : single AND of all four gate outputs
 *   - SR latch         : set/reset latch
 *   - D flip-flop      : edge-triggered D flip-flop with set/reset
 *   - JK flip-flop     : edge-triggered JK flip-flop
 *   - Transparent latch : level-sensitive D latch
 *
 * Each cell has four data input selectors (D1S-D4S), four gate
 * logic-select registers (GLS0-GLS3) controlling which data inputs
 * feed each gate in true or complemented form, a polarity register
 * for gate and output inversion, and a control register for mode
 * selection and interrupt configuration.
 *
 * Register access uses an indexed scheme: write the module number
 * (0-7) to CLCSELECT, then read/write the CLCnXXX registers at
 * their fixed addresses.  CLCDATA provides a read-only snapshot of
 * all eight CLC outputs in a single byte.
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_CLC_H
#define PIC18F_Q84_CLC_H

/* ===================================================================
 * CLCDATA - CLC Data Output Register (read-only)
 * Address: 0x00D0
 *
 * Provides a consolidated read-only view of the current logic output
 * state of all eight CLC modules.  Each bit reflects the final
 * (post-polarity) output of the corresponding CLC.  This register
 * allows software to sample multiple CLC outputs atomically in a
 * single read, avoiding the need to select each module individually.
 * ================================================================ */
#define CLCDATA SFR8(0x00D0)

/* Bit 7 - CLC8OUT: CLC8 output state (read-only)
 *   1 = CLC8 output is logic high.
 *   0 = CLC8 output is logic low.                                  */
#define CLCDATA_CLC8OUT 7

/* Bit 6 - CLC7OUT: CLC7 output state (read-only)                   */
#define CLCDATA_CLC7OUT 6

/* Bit 5 - CLC6OUT: CLC6 output state (read-only)                   */
#define CLCDATA_CLC6OUT 5

/* Bit 4 - CLC5OUT: CLC5 output state (read-only)                   */
#define CLCDATA_CLC5OUT 4

/* Bit 3 - CLC4OUT: CLC4 output state (read-only)                   */
#define CLCDATA_CLC4OUT 3

/* Bit 2 - CLC3OUT: CLC3 output state (read-only)                   */
#define CLCDATA_CLC3OUT 2

/* Bit 1 - CLC2OUT: CLC2 output state (read-only)                   */
#define CLCDATA_CLC2OUT 1

/* Bit 0 - CLC1OUT: CLC1 output state (read-only)                   */
#define CLCDATA_CLC1OUT 0

/* ===================================================================
 * CLCSELECT - CLC Module Select Register
 * Address: 0x00D1
 *
 * Selects which of the eight CLC modules (CLC1-CLC8) is accessed
 * through the indexed register set at addresses 0x00D2-0x00DB.
 * After writing a value to CLCSELECT, subsequent reads and writes
 * to CLCnCON through CLCnGLS3 operate on the selected module.
 *
 * Changing CLCSELECT does not affect the operational state of any
 * CLC module; it only redirects the register window.
 * ================================================================ */
#define CLCSELECT SFR8(0x00D1)

/* Bits 7:3 - Reserved: read as 0, write as 0.                      */

/* Bits 2:0 - SLCT[2:0]: Module Select
 *   Selects the active CLC module for the indexed register window:
 *   000 = CLC1
 *   001 = CLC2
 *   010 = CLC3
 *   011 = CLC4
 *   100 = CLC5
 *   101 = CLC6
 *   110 = CLC7
 *   111 = CLC8                                                      */
#define CLCSELECT_SLCT2 2
#define CLCSELECT_SLCT1 1
#define CLCSELECT_SLCT0 0

/* SLCT field mask and position                                      */
#define CLCSELECT_SLCT_MASK 0x07 /* bits 2:0 */
#define CLCSELECT_SLCT_POS 0

/* Named module-select values for SLCT[2:0]                          */
#define SLCT_CLC1 0x00
#define SLCT_CLC2 0x01
#define SLCT_CLC3 0x02
#define SLCT_CLC4 0x03
#define SLCT_CLC5 0x04
#define SLCT_CLC6 0x05
#define SLCT_CLC7 0x06
#define SLCT_CLC8 0x07

/* ===================================================================
 * CLCnCON - CLC Control Register (indexed by CLCSELECT)
 * Address: 0x00D2
 *
 * Controls the enable state, output interrupt edge selection, and
 * logic function mode of the currently selected CLC module.
 *
 * The MODE field determines the logic topology:
 *
 *   AND-OR mode (000):
 *     Four AND gates; outputs OR'd to produce the final result.
 *     Classic sum-of-products (SOP) implementation.
 *
 *   OR-XOR mode (001):
 *     Four OR gates; outputs fed through an XOR chain.
 *     Useful for parity generation and comparison logic.
 *
 *   4-input AND mode (010):
 *     All four gate outputs AND'd together.
 *     Implements a single wide AND function.
 *
 *   SR latch mode (011):
 *     Gate 1 output = S (Set), Gate 2 output = R (Reset).
 *     Gates 3 and 4 are unused.
 *
 *   D flip-flop mode (100):
 *     Gate 1 = D (data), Gate 2 = CLK (clock edge),
 *     Gate 3 = R (asynchronous reset),
 *     Gate 4 = S (asynchronous set).
 *
 *   JK flip-flop mode (101):
 *     Gate 1 = J, Gate 2 = CLK, Gate 3 = K.
 *     Gate 4 is unused.
 *
 *   Transparent D latch mode (110):
 *     Gate 1 = D (data), Gate 2 = G (gate/enable).
 *     Output follows D while G is high; latched when G goes low.
 *     Gates 3 and 4 are unused.
 *
 *   Mode 111 is reserved.
 * ================================================================ */
#define CLCnCON SFR8(0x00D2)

/* Bit 7 - EN: CLC Module Enable
 *   1 = CLC module is enabled; logic function is active and the
 *       output drives the selected PPS output and/or interrupts.
 *   0 = CLC module is disabled; output is forced low, no interrupts
 *       are generated, and the module draws minimal current.        */
#define CLCnCON_EN 7

/* Bit 6 - Reserved: read as 0, write as 0.                         */

/* Bit 5 - OUT: CLC Output (read-only)
 *   Reflects the current logic level of the CLC output after all
 *   gate logic and output polarity inversion have been applied.
 *   1 = Output is logic high.
 *   0 = Output is logic low.                                       */
#define CLCnCON_OUT 5

/* Bit 4 - INTP: Interrupt on Positive Edge
 *   1 = An interrupt flag is set on the rising edge (low-to-high
 *       transition) of the CLC output.
 *   0 = Rising-edge interrupts disabled.                            */
#define CLCnCON_INTP 4

/* Bit 3 - INTN: Interrupt on Negative Edge
 *   1 = An interrupt flag is set on the falling edge (high-to-low
 *       transition) of the CLC output.
 *   0 = Falling-edge interrupts disabled.
 *   Note: INTP and INTN may both be set simultaneously to generate
 *   an interrupt on either edge.                                    */
#define CLCnCON_INTN 3

/* Bits 2:0 - MODE[2:0]: Logic Function Mode Select
 *   See register description above for mode assignments.            */
#define CLCnCON_MODE2 2
#define CLCnCON_MODE1 1
#define CLCnCON_MODE0 0

/* MODE field mask and position                                      */
#define CLCnCON_MODE_MASK 0x07 /* bits 2:0 */
#define CLCnCON_MODE_POS 0

/* Named mode values for MODE[2:0]                                   */
#define MODE_AND_OR 0x00   /* AND-OR: sum-of-products          */
#define MODE_OR_XOR 0x01   /* OR-XOR: parity / comparison      */
#define MODE_AND 0x02      /* 4-input AND                      */
#define MODE_SR_LATCH 0x03 /* SR latch                         */
#define MODE_D_FF 0x04     /* D flip-flop with set/reset       */
#define MODE_JK_FF 0x05    /* JK flip-flop                     */
#define MODE_D_LATCH 0x06  /* Transparent D latch              */

/* ===================================================================
 * CLCnPOL - CLC Polarity Control Register (indexed by CLCSELECT)
 * Address: 0x00D3
 *
 * Controls inversion of the final CLC output and of each individual
 * gate output.  Polarity inversion is applied after the gate logic
 * but before the signal enters the combining stage (OR, XOR, AND,
 * or latch/flip-flop inputs).
 *
 * The output polarity bit (POL) inverts the final combined result
 * after the logic function, effectively providing a NAND-NOR,
 * NOR-XNOR, NAND, or inverted latch output as needed.
 * ================================================================ */
#define CLCnPOL SFR8(0x00D3)

/* Bit 7 - POL: Output Polarity
 *   1 = Final CLC output is inverted (active-low output).
 *   0 = Final CLC output is non-inverted (active-high output).     */
#define CLCnPOL_POL 7

/* Bits 6:4 - Reserved: read as 0, write as 0.                      */

/* Bit 3 - G4POL: Gate 4 Output Polarity
 *   1 = Gate 4 output is inverted before entering the logic function.
 *   0 = Gate 4 output is non-inverted.                              */
#define CLCnPOL_G4POL 3

/* Bit 2 - G3POL: Gate 3 Output Polarity
 *   1 = Gate 3 output is inverted.
 *   0 = Gate 3 output is non-inverted.                              */
#define CLCnPOL_G3POL 2

/* Bit 1 - G2POL: Gate 2 Output Polarity
 *   1 = Gate 2 output is inverted.
 *   0 = Gate 2 output is non-inverted.                              */
#define CLCnPOL_G2POL 1

/* Bit 0 - G1POL: Gate 1 Output Polarity
 *   1 = Gate 1 output is inverted.
 *   0 = Gate 1 output is non-inverted.                              */
#define CLCnPOL_G1POL 0

/* Combined gate polarity mask (bits 3:0)                            */
#define CLCnPOL_GxPOL_MASK 0x0F

/* ===================================================================
 * CLCnSEL0 - CLC Data 1 Input Source Select (indexed by CLCSELECT)
 * Address: 0x00D4
 *
 * Selects the signal source for Data Input 1 of the currently
 * selected CLC module.  The available sources include CLCIN pins
 * (via PPS), other CLC outputs, timer outputs, comparator outputs,
 * UART TX/RX, SPI/I2C signals, PWM outputs, and other peripherals.
 * The specific mapping of D1S values to signal sources is defined
 * in the "CLC Input Selection" table of the data sheet.
 * ================================================================ */
#define CLCnSEL0 SFR8(0x00D4)

/* Bits 7:0 - D1S[7:0]: Data 1 Input Source Select
 *   Selects the peripheral or pin signal routed to Data Input 1.
 *   Refer to DS40002213D Table "CLCn Input Selection" for the
 *   complete mapping of source codes to signal names.               */
#define CLCnSEL0_D1S_MASK 0xFF /* bits 7:0 */
#define CLCnSEL0_D1S_POS 0

/* ===================================================================
 * CLCnSEL1 - CLC Data 2 Input Source Select (indexed by CLCSELECT)
 * Address: 0x00D5
 *
 * Selects the signal source for Data Input 2.
 * ================================================================ */
#define CLCnSEL1 SFR8(0x00D5)

/* Bits 7:0 - D2S[7:0]: Data 2 Input Source Select                  */
#define CLCnSEL1_D2S_MASK 0xFF /* bits 7:0 */
#define CLCnSEL1_D2S_POS 0

/* ===================================================================
 * CLCnSEL2 - CLC Data 3 Input Source Select (indexed by CLCSELECT)
 * Address: 0x00D6
 *
 * Selects the signal source for Data Input 3.
 * ================================================================ */
#define CLCnSEL2 SFR8(0x00D6)

/* Bits 7:0 - D3S[7:0]: Data 3 Input Source Select                  */
#define CLCnSEL2_D3S_MASK 0xFF /* bits 7:0 */
#define CLCnSEL2_D3S_POS 0

/* ===================================================================
 * CLCnSEL3 - CLC Data 4 Input Source Select (indexed by CLCSELECT)
 * Address: 0x00D7
 *
 * Selects the signal source for Data Input 4.
 * ================================================================ */
#define CLCnSEL3 SFR8(0x00D7)

/* Bits 7:0 - D4S[7:0]: Data 4 Input Source Select                  */
#define CLCnSEL3_D4S_MASK 0xFF /* bits 7:0 */
#define CLCnSEL3_D4S_POS 0

/* ===================================================================
 * CLCnGLS0 - CLC Gate 1 Logic Select (indexed by CLCSELECT)
 * Address: 0x00D8
 *
 * Controls which of the four data inputs are included in Gate 1's
 * logic function, and whether each input is included in its true
 * (non-inverted) or negated (inverted) form.
 *
 * For each data input Dx (x = 1..4), two bits control inclusion:
 *   GxDxT = 1 : include the TRUE (non-inverted) value of Data x
 *   GxDxN = 1 : include the NEGATED (inverted) value of Data x
 *
 * If both T and N bits for a given data input are 0, that input
 * does not participate in the gate function (effectively removed).
 *
 * If both T and N are set, both true and complement are included
 * in the gate, which forces the gate output high in AND-OR mode
 * (since Dx OR NOT(Dx) = 1).
 *
 * The gate function depends on the selected MODE:
 *   AND-OR mode : Gate inputs are AND'd together
 *   OR-XOR mode : Gate inputs are OR'd together
 *   AND mode    : Gate output feeds the 4-input AND
 *   Latch/FF    : Gate output drives the assigned latch/FF input
 * ================================================================ */
#define CLCnGLS0 SFR8(0x00D8)

/* Bit 7 - G1D4T: Gate 1, Data 4 True
 *   1 = Non-inverted Data 4 is included in Gate 1's logic.
 *   0 = Non-inverted Data 4 is excluded from Gate 1.               */
#define CLCnGLS0_G1D4T 7

/* Bit 6 - G1D4N: Gate 1, Data 4 Negated
 *   1 = Inverted Data 4 is included in Gate 1's logic.
 *   0 = Inverted Data 4 is excluded from Gate 1.                   */
#define CLCnGLS0_G1D4N 6

/* Bit 5 - G1D3T: Gate 1, Data 3 True                               */
#define CLCnGLS0_G1D3T 5

/* Bit 4 - G1D3N: Gate 1, Data 3 Negated                            */
#define CLCnGLS0_G1D3N 4

/* Bit 3 - G1D2T: Gate 1, Data 2 True                               */
#define CLCnGLS0_G1D2T 3

/* Bit 2 - G1D2N: Gate 1, Data 2 Negated                            */
#define CLCnGLS0_G1D2N 2

/* Bit 1 - G1D1T: Gate 1, Data 1 True                               */
#define CLCnGLS0_G1D1T 1

/* Bit 0 - G1D1N: Gate 1, Data 1 Negated                            */
#define CLCnGLS0_G1D1N 0

/* ===================================================================
 * CLCnGLS1 - CLC Gate 2 Logic Select (indexed by CLCSELECT)
 * Address: 0x00D9
 *
 * Controls data input inclusion for Gate 2, using the same
 * true/negated bit-pair scheme as CLCnGLS0.
 * ================================================================ */
#define CLCnGLS1 SFR8(0x00D9)

/* Bit 7 - G2D4T: Gate 2, Data 4 True                               */
#define CLCnGLS1_G2D4T 7

/* Bit 6 - G2D4N: Gate 2, Data 4 Negated                            */
#define CLCnGLS1_G2D4N 6

/* Bit 5 - G2D3T: Gate 2, Data 3 True                               */
#define CLCnGLS1_G2D3T 5

/* Bit 4 - G2D3N: Gate 2, Data 3 Negated                            */
#define CLCnGLS1_G2D3N 4

/* Bit 3 - G2D2T: Gate 2, Data 2 True                               */
#define CLCnGLS1_G2D2T 3

/* Bit 2 - G2D2N: Gate 2, Data 2 Negated                            */
#define CLCnGLS1_G2D2N 2

/* Bit 1 - G2D1T: Gate 2, Data 1 True                               */
#define CLCnGLS1_G2D1T 1

/* Bit 0 - G2D1N: Gate 2, Data 1 Negated                            */
#define CLCnGLS1_G2D1N 0

/* ===================================================================
 * CLCnGLS2 - CLC Gate 3 Logic Select (indexed by CLCSELECT)
 * Address: 0x00DA
 *
 * Controls data input inclusion for Gate 3, using the same
 * true/negated bit-pair scheme as CLCnGLS0.
 * ================================================================ */
#define CLCnGLS2 SFR8(0x00DA)

/* Bit 7 - G3D4T: Gate 3, Data 4 True                               */
#define CLCnGLS2_G3D4T 7

/* Bit 6 - G3D4N: Gate 3, Data 4 Negated                            */
#define CLCnGLS2_G3D4N 6

/* Bit 5 - G3D3T: Gate 3, Data 3 True                               */
#define CLCnGLS2_G3D3T 5

/* Bit 4 - G3D3N: Gate 3, Data 3 Negated                            */
#define CLCnGLS2_G3D3N 4

/* Bit 3 - G3D2T: Gate 3, Data 2 True                               */
#define CLCnGLS2_G3D2T 3

/* Bit 2 - G3D2N: Gate 3, Data 2 Negated                            */
#define CLCnGLS2_G3D2N 2

/* Bit 1 - G3D1T: Gate 3, Data 1 True                               */
#define CLCnGLS2_G3D1T 1

/* Bit 0 - G3D1N: Gate 3, Data 1 Negated                            */
#define CLCnGLS2_G3D1N 0

/* ===================================================================
 * CLCnGLS3 - CLC Gate 4 Logic Select (indexed by CLCSELECT)
 * Address: 0x00DB
 *
 * Controls data input inclusion for Gate 4, using the same
 * true/negated bit-pair scheme as CLCnGLS0.
 * ================================================================ */
#define CLCnGLS3 SFR8(0x00DB)

/* Bit 7 - G4D4T: Gate 4, Data 4 True                               */
#define CLCnGLS3_G4D4T 7

/* Bit 6 - G4D4N: Gate 4, Data 4 Negated                            */
#define CLCnGLS3_G4D4N 6

/* Bit 5 - G4D3T: Gate 4, Data 3 True                               */
#define CLCnGLS3_G4D3T 5

/* Bit 4 - G4D3N: Gate 4, Data 3 Negated                            */
#define CLCnGLS3_G4D3N 4

/* Bit 3 - G4D2T: Gate 4, Data 2 True                               */
#define CLCnGLS3_G4D2T 3

/* Bit 2 - G4D2N: Gate 4, Data 2 Negated                            */
#define CLCnGLS3_G4D2N 2

/* Bit 1 - G4D1T: Gate 4, Data 1 True                               */
#define CLCnGLS3_G4D1T 1

/* Bit 0 - G4D1N: Gate 4, Data 1 Negated                            */
#define CLCnGLS3_G4D1N 0

#endif /* PIC18F_Q84_CLC_H */
