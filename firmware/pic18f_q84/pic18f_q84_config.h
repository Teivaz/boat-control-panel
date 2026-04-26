/**
 * @file pic18f_q84_config.h
 * @brief PIC18F27/47/57Q84 Device Configuration Word definitions.
 *
 * Configuration words are programmed at device programming time (via ICSP,
 * JTAG, or a device programmer) and control fundamental device behaviour
 * that cannot be changed at run-time.  They reside in a special region of
 * program memory at addresses 0x300000-0x30000A.
 *
 * On the PIC18F-Q84 family there are eleven configuration registers
 * (CONFIG1-CONFIG11) plus a read-only Device ID and Revision ID.
 *
 * The configuration words are typically set via compiler pragmas or
 * assembler directives rather than via SFR writes.  The definitions in
 * this header are provided so that firmware can READ the configuration
 * (e.g., to verify settings at boot) using table-read instructions.
 *
 * Typical usage with XC8 compiler:
 *
 *   #pragma config FEXTOSC = HS     // High-speed crystal
 *   #pragma config RSTOSC = HFINTOSC_1MHZ
 *   #pragma config WDTE = OFF       // WDT disabled
 *   #pragma config LVP = ON         // Low-voltage programming enabled
 *
 * Register summary:
 *
 *   CONFIG1  (0x300000) - External oscillator / reset oscillator select
 *   CONFIG2  (0x300001) - FSCM, clock switch, PPS, clock out, power-up timer
 *   CONFIG3  (0x300002) - Multi-vector, IVT lock, MCLR, BOR, PPS
 *   CONFIG4  (0x300003) - JTAG, stack reset, XINST, debug, LVP, ZCD, BORV
 *   CONFIG5  (0x300004) - WDT period select (configuration default)
 *   CONFIG6  (0x300005) - WDT clock source, window, enable
 *   CONFIG7  (0x300006) - SAF enable, boot block size
 *   CONFIG8  (0x300007) - Boot partition / boot block enable
 *   CONFIG9  (0x300008) - Write protection bits
 *   CONFIG10 (0x300009) - Code protection, open-drain, boot pin select
 *   CONFIG11 (0x30000A) - Boot/code security
 *
 *   DEVICEID   (0x3FFFFE) - Factory-programmed device identifier (16-bit)
 *   REVISIONID (0x3FFFFC) - Factory-programmed silicon revision (16-bit)
 *
 * All addresses are absolute program-memory byte addresses.
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *
 * @note This file is included automatically by pic18f_q84.h.
 *       It relies on SFR8() and SFR16() macros defined there.
 */

#ifndef PIC18F_Q84_CONFIG_H
#define PIC18F_Q84_CONFIG_H

/* ****************************************************************************
 *  Configuration Word Base Addresses
 * ****************************************************************************
 *
 *  These addresses are in the configuration address space (0x300000+).
 *  They are accessed via table-read (TBLRD) instructions; normal SFR
 *  read/write does not apply.  The SFR8() macro is provided here for
 *  consistency, but on most toolchains configuration words are read via
 *  special built-in functions or table-read inline assembly.
 * ***************************************************************************/

#define CONFIG1_ADDR 0x300000u
#define CONFIG2_ADDR 0x300001u
#define CONFIG3_ADDR 0x300002u
#define CONFIG4_ADDR 0x300003u
#define CONFIG5_ADDR 0x300004u
#define CONFIG6_ADDR 0x300005u
#define CONFIG7_ADDR 0x300006u
#define CONFIG8_ADDR 0x300007u
#define CONFIG9_ADDR 0x300008u
#define CONFIG10_ADDR 0x300009u
#define CONFIG11_ADDR 0x30000Au

/* ============================================================================
 *  CONFIG1 - Oscillator Selection
 * ============================================================================
 *  Address: 0x300000   (8-bit)
 *
 *  Selects the external oscillator mode and the oscillator used after
 *  reset (before any software reconfiguration).
 *
 *    Bit 7:5 - RSTOSC[2:0]  Reset Oscillator Selection
 *              Selects which clock source is active at power-on reset.
 *              000 = Reserved
 *              001 = EXTOSC operating with 4x PLL
 *              010 = EXTOSC (external oscillator, no PLL)
 *              011 = Reserved
 *              100 = HFINTOSC with OSCFRQ setting
 *              101 = Reserved
 *              110 = HFINTOSC at 1 MHz (default)
 *              111 = LFINTOSC at 31.0 kHz
 *
 *    Bit 4:3 - Reserved (read as 1)
 *
 *    Bit 2:0 - FEXTOSC[2:0]  External Oscillator Mode Selection
 *              Selects the external oscillator circuit type (when used).
 *              000 = LP crystal (low-power, <200 kHz)
 *              001 = XT crystal (medium freq, 200 kHz - 4 MHz)
 *              010 = HS crystal (high-speed, 4 MHz - 20 MHz)
 *              011 = Reserved
 *              100 = Reserved
 *              101 = EC with CLKOUT on OSC2 pin
 *              110 = EC (external clock, OSC2 as I/O)
 *              111 = EC with 4x PLL, CLKOUT on OSC2
 * ========================================================================= */

/* -- Field positions ------------------------------------------------------ */
#define CONFIG1_RSTOSC_POS 5  /**< Reset oscillator field [7:5]     */
#define CONFIG1_FEXTOSC_POS 0 /**< External osc mode field [2:0]    */

/* -- Field masks ---------------------------------------------------------- */
#define CONFIG1_RSTOSC_MASK 0xE0u  /**< RSTOSC field mask (bits 7:5)     */
#define CONFIG1_FEXTOSC_MASK 0x07u /**< FEXTOSC field mask (bits 2:0)    */

/* -- RSTOSC field values -------------------------------------------------- */
#define CONFIG1_RSTOSC_EXTOSC_4XPLL (0x01u << CONFIG1_RSTOSC_POS)  /**< EXTOSC with 4x PLL                */
#define CONFIG1_RSTOSC_EXTOSC (0x02u << CONFIG1_RSTOSC_POS)        /**< EXTOSC (no PLL)                   */
#define CONFIG1_RSTOSC_HFINTOSC (0x04u << CONFIG1_RSTOSC_POS)      /**< HFINTOSC per OSCFRQ               */
#define CONFIG1_RSTOSC_HFINTOSC_1MHZ (0x06u << CONFIG1_RSTOSC_POS) /**< HFINTOSC at 1 MHz (default)       */
#define CONFIG1_RSTOSC_LFINTOSC (0x07u << CONFIG1_RSTOSC_POS)      /**< LFINTOSC at 31 kHz                */

/* -- FEXTOSC field values ------------------------------------------------- */
#define CONFIG1_FEXTOSC_LP 0x00u        /**< LP crystal oscillator            */
#define CONFIG1_FEXTOSC_XT 0x01u        /**< XT crystal oscillator            */
#define CONFIG1_FEXTOSC_HS 0x02u        /**< HS crystal oscillator            */
#define CONFIG1_FEXTOSC_EC_CLKOUT 0x05u /**< EC mode, CLKOUT on OSC2         */
#define CONFIG1_FEXTOSC_EC 0x06u        /**< EC mode, OSC2 as I/O            */
#define CONFIG1_FEXTOSC_EC_PLL 0x07u    /**< EC mode with 4x PLL             */

/* ============================================================================
 *  CONFIG2 - Clock Configuration
 * ============================================================================
 *  Address: 0x300001   (8-bit)
 *
 *    Bit 7   - FCMENS   Fail-Safe Clock Monitor Enable (Secondary)
 *              1 = FSCM for secondary oscillator enabled
 *              0 = FSCM for secondary oscillator disabled
 *
 *    Bit 6   - FCMENP   Fail-Safe Clock Monitor Enable (Primary)
 *              1 = FSCM for primary oscillator enabled
 *              0 = FSCM for primary oscillator disabled
 *
 *    Bit 5   - FCMEN    FOSC Fail-Safe Clock Monitor Enable
 *              1 = FSCM enabled (switches to HFINTOSC on clock failure)
 *              0 = FSCM disabled
 *
 *    Bit 4   - CSWEN    Clock Switching Enable
 *              1 = Writing to NOSC and NDIV is allowed
 *              0 = NOSC and NDIV bits cannot be changed by software
 *
 *    Bit 3   - PR1WAY   Peripheral Pin Select One-Way Control
 *              1 = PPSLOCKED bit can only be set once after unlock
 *              0 = PPSLOCKED bit can be set and cleared repeatedly
 *
 *    Bit 2   - CLKOUTEN Clock Out Enable
 *              1 = CLKOUT function is disabled; I/O on the pin
 *              0 = System clock (FOSC/4) output on CLKOUT pin
 *
 *    Bit 1:0 - PWRTS[1:0]  Power-Up Timer Selection
 *              00 = Power-up timer disabled
 *              01 = PWRT set to 64 ms
 *              10 = PWRT set to 1 s
 *              11 = PWRT set to 2 s
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG2_FCMENS_POS 7
#define CONFIG2_FCMENP_POS 6
#define CONFIG2_FCMEN_POS 5
#define CONFIG2_CSWEN_POS 4
#define CONFIG2_PR1WAY_POS 3
#define CONFIG2_CLKOUTEN_POS 2
#define CONFIG2_PWRTS_POS 0

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG2_FCMENS (1u << CONFIG2_FCMENS_POS)     /**< 0x80 - Secondary FSCM enable      */
#define CONFIG2_FCMENP (1u << CONFIG2_FCMENP_POS)     /**< 0x40 - Primary FSCM enable        */
#define CONFIG2_FCMEN (1u << CONFIG2_FCMEN_POS)       /**< 0x20 - FOSC FSCM enable           */
#define CONFIG2_CSWEN (1u << CONFIG2_CSWEN_POS)       /**< 0x10 - Clock switch enable        */
#define CONFIG2_PR1WAY (1u << CONFIG2_PR1WAY_POS)     /**< 0x08 - PPS one-way lock           */
#define CONFIG2_CLKOUTEN (1u << CONFIG2_CLKOUTEN_POS) /**< 0x04 - 1=CLKOUT disabled          */
#define CONFIG2_PWRTS_MASK 0x03u                      /**< Power-up timer field [1:0]       */

/* -- PWRTS field values --------------------------------------------------- */
#define CONFIG2_PWRTS_OFF 0x00u  /**< Power-up timer disabled          */
#define CONFIG2_PWRTS_64MS 0x01u /**< 64 ms power-up timer             */
#define CONFIG2_PWRTS_1S 0x02u   /**< 1 second power-up timer          */
#define CONFIG2_PWRTS_2S 0x03u   /**< 2 second power-up timer          */

/* ============================================================================
 *  CONFIG3 - Supervisor Configuration
 * ============================================================================
 *  Address: 0x300002   (8-bit)
 *
 *    Bit 7   - MVECEN   Multi-Vector Interrupt Enable
 *              1 = Vectored interrupts enabled; IVT is used
 *              0 = Legacy interrupt mode (single vector at 0x0008/0x0018)
 *
 *    Bit 6   - IVT1WAY  IVT One-Way Lock
 *              1 = IVTLOCKED can only be set once after unlock
 *              0 = IVTLOCKED can be set and cleared repeatedly
 *
 *    Bit 5   - MCLRE    Master Clear Enable
 *              1 = RE3 pin functions as MCLR (active-low reset input)
 *              0 = RE3 pin functions as digital input (MCLR internally tied)
 *
 *    Bit 4   - Reserved (read as 1)
 *
 *    Bit 3:2 - BOREN[1:0]  Brown-out Reset Enable
 *              00 = BOR disabled
 *              01 = BOR enabled in Run mode only; disabled in Sleep
 *              10 = BOR always enabled (hardware controlled)
 *              11 = BOR enabled; can be disabled in software (SBOREN)
 *
 *    Bit 1   - LPBOREN  Low-Power Brown-out Reset Enable
 *              1 = Low-power BOR circuit enabled
 *              0 = Low-power BOR disabled
 *
 *    Bit 0   - PPS1WAY  PPS One-Way Control (alternate location)
 *              1 = PPSLOCKED can only be set once
 *              0 = PPSLOCKED can be set and cleared repeatedly
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG3_MVECEN_POS 7
#define CONFIG3_IVT1WAY_POS 6
#define CONFIG3_MCLRE_POS 5
#define CONFIG3_BOREN_POS 2
#define CONFIG3_LPBOREN_POS 1
#define CONFIG3_PPS1WAY_POS 0

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG3_MVECEN (1u << CONFIG3_MVECEN_POS)   /**< 0x80 - 1=multi-vector interrupts  */
#define CONFIG3_IVT1WAY (1u << CONFIG3_IVT1WAY_POS) /**< 0x40 - 1=IVT one-way lock        */
#define CONFIG3_MCLRE (1u << CONFIG3_MCLRE_POS)     /**< 0x20 - 1=MCLR enabled on RE3     */
#define CONFIG3_BOREN_MASK 0x0Cu                    /**< BOR enable field [3:2]           */
#define CONFIG3_LPBOREN (1u << CONFIG3_LPBOREN_POS) /**< 0x02 - 1=low-power BOR enabled    */
#define CONFIG3_PPS1WAY (1u << CONFIG3_PPS1WAY_POS) /**< 0x01 - 1=PPS one-way lock         */

/* -- BOREN field values --------------------------------------------------- */
#define CONFIG3_BOREN_OFF (0x00u << CONFIG3_BOREN_POS)      /**< BOR disabled                     */
#define CONFIG3_BOREN_RUN_ONLY (0x01u << CONFIG3_BOREN_POS) /**< BOR enabled in Run mode only     */
#define CONFIG3_BOREN_ALWAYS (0x02u << CONFIG3_BOREN_POS)   /**< BOR always enabled               */
#define CONFIG3_BOREN_SBOREN (0x03u << CONFIG3_BOREN_POS)   /**< BOR software controlled (SBOREN) */

/* ============================================================================
 *  CONFIG4 - System Configuration
 * ============================================================================
 *  Address: 0x300003   (8-bit)
 *
 *    Bit 7   - JTAGEN   JTAG Enable
 *              1 = JTAG port is enabled (uses PORTB pins for TDI/TDO/TCK/TMS)
 *              0 = JTAG is disabled; PORTB pins available as I/O
 *
 *    Bit 6   - Reserved (read as 1)
 *
 *    Bit 5   - STVREN   Stack Overflow/Underflow Reset Enable
 *              1 = Stack overflow or underflow causes a device reset
 *              0 = Stack overflow/underflow does not cause reset
 *
 *    Bit 4   - XINST    Extended Instruction Set Enable
 *              1 = Extended instruction set and indexed addressing enabled
 *              0 = Legacy instruction set only
 *
 *    Bit 3   - DEBUG    Background Debugger Enable
 *              1 = Background debugger enabled (ICD/REAL ICE attached)
 *              0 = Background debugger disabled
 *
 *    Bit 2   - LVP      Low-Voltage Programming Enable
 *              1 = Low-voltage programming enabled (PGM pin is LVP entry)
 *              0 = HV on MCLR required for programming
 *
 *    Bit 1:0 - BORV[1:0]  Brown-out Reset Voltage Selection
 *              00 = BOR voltage set to highest trip point (~2.85V)
 *              01 = BOR voltage set to low trip point (~2.45V)
 *              10 = BOR voltage set to lowest trip point (~1.9V)
 *              11 = Reserved
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG4_JTAGEN_POS 7
#define CONFIG4_STVREN_POS 5
#define CONFIG4_XINST_POS 4
#define CONFIG4_DEBUG_POS 3
#define CONFIG4_LVP_POS 2
#define CONFIG4_BORV_POS 0

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG4_JTAGEN (1u << CONFIG4_JTAGEN_POS) /**< 0x80 - 1=JTAG enabled             */
#define CONFIG4_STVREN (1u << CONFIG4_STVREN_POS) /**< 0x20 - 1=stack over/underflow reset*/
#define CONFIG4_XINST (1u << CONFIG4_XINST_POS)   /**< 0x10 - 1=extended instruction set  */
#define CONFIG4_DEBUG (1u << CONFIG4_DEBUG_POS)   /**< 0x08 - 1=debug mode enabled        */
#define CONFIG4_LVP (1u << CONFIG4_LVP_POS)       /**< 0x04 - 1=low-voltage programming   */
#define CONFIG4_BORV_MASK 0x03u                   /**< BOR voltage field [1:0]          */

/* -- BORV field values ---------------------------------------------------- */
#define CONFIG4_BORV_HIGH 0x00u   /**< Highest BOR trip point (~2.85V)  */
#define CONFIG4_BORV_LOW 0x01u    /**< Low BOR trip point (~2.45V)      */
#define CONFIG4_BORV_LOWEST 0x02u /**< Lowest BOR trip point (~1.9V)    */

/* ============================================================================
 *  CONFIG5 - Watchdog Timer Period Selection (Configuration Default)
 * ============================================================================
 *  Address: 0x300004   (8-bit)
 *
 *  Sets the default WDT prescaler/period that is loaded into WDTCON0
 *  at power-on reset.  The actual WDT period depends on the clock
 *  source selected in CONFIG6.
 *
 *    Bit 7:5 - Reserved (read as 1)
 *
 *    Bit 4:0 - WDTCPS[4:0]  WDT Period Select
 *              Selects the nominal WDT timeout period.  Values assume the
 *              31 kHz LFINTOSC source (~1 ms base period).
 *              00000 = 1:1        (~1 ms)
 *              00001 = 1:2        (~2 ms)
 *              00010 = 1:4        (~4 ms)
 *              ...
 *              10010 = 1:262144   (~256 s)
 *              10011 = Software controlled (WDTCON0.PS value used)
 *              Other values reserved.
 * ========================================================================= */

/* -- Field position and mask ---------------------------------------------- */
#define CONFIG5_WDTCPS_POS 0
#define CONFIG5_WDTCPS_MASK 0x1Fu /**< WDTCPS field [4:0]              */

/* -- Selected WDTCPS values ----------------------------------------------- */
#define CONFIG5_WDTCPS_1MS 0x00u   /**< ~1 ms WDT period                */
#define CONFIG5_WDTCPS_2MS 0x01u   /**< ~2 ms WDT period                */
#define CONFIG5_WDTCPS_4MS 0x02u   /**< ~4 ms WDT period                */
#define CONFIG5_WDTCPS_8MS 0x03u   /**< ~8 ms WDT period                */
#define CONFIG5_WDTCPS_16MS 0x04u  /**< ~16 ms WDT period               */
#define CONFIG5_WDTCPS_32MS 0x05u  /**< ~32 ms WDT period               */
#define CONFIG5_WDTCPS_64MS 0x06u  /**< ~64 ms WDT period               */
#define CONFIG5_WDTCPS_128MS 0x07u /**< ~128 ms WDT period              */
#define CONFIG5_WDTCPS_256MS 0x08u /**< ~256 ms WDT period              */
#define CONFIG5_WDTCPS_512MS 0x09u /**< ~512 ms WDT period              */
#define CONFIG5_WDTCPS_1S 0x0Au    /**< ~1 s WDT period                 */
#define CONFIG5_WDTCPS_2S 0x0Bu    /**< ~2 s WDT period                 */
#define CONFIG5_WDTCPS_4S 0x0Cu    /**< ~4 s WDT period                 */
#define CONFIG5_WDTCPS_8S 0x0Du    /**< ~8 s WDT period                 */
#define CONFIG5_WDTCPS_16S 0x0Eu   /**< ~16 s WDT period                */
#define CONFIG5_WDTCPS_32S 0x0Fu   /**< ~32 s WDT period                */
#define CONFIG5_WDTCPS_64S 0x10u   /**< ~64 s WDT period                */
#define CONFIG5_WDTCPS_128S 0x11u  /**< ~128 s WDT period               */
#define CONFIG5_WDTCPS_256S 0x12u  /**< ~256 s WDT period               */
#define CONFIG5_WDTCPS_SW 0x13u    /**< Software controlled              */

/* ============================================================================
 *  CONFIG6 - Watchdog Timer Configuration
 * ============================================================================
 *  Address: 0x300005   (8-bit)
 *
 *    Bit 7:5 - WDTCCS[2:0]  WDT Input Clock Source Select
 *              000 = LFINTOSC (31 kHz)
 *              001 = HFINTOSC (divided, ~31.25 kHz)
 *              010 = Reserved
 *              011 = Reserved
 *              100 = SOSC / T1OSC (if available)
 *              101-110 = Reserved
 *              111 = Software controlled (WDTCON1 value used)
 *
 *    Bit 4:2 - WDTCWS[2:0]  WDT Window Select
 *              Defines the open-window percentage for windowed WDT mode.
 *              000 = Window = 12.5% of WDT period (open at end)
 *              001 = Window = 25%
 *              010 = Window = 37.5%
 *              011 = Window = 50%
 *              100 = Window = 62.5%
 *              101 = Window = 75%
 *              110 = Window = 87.5%
 *              111 = Window = 100% (windowed mode disabled, always open)
 *
 *    Bit 1:0 - WDTE[1:0]  Watchdog Timer Enable
 *              00 = WDT disabled; SWDTEN is ignored
 *              01 = WDT controlled by software (SWDTEN in WDTCON0)
 *              10 = WDT enabled while running; disabled in Sleep
 *              11 = WDT always enabled
 * ========================================================================= */

/* -- Field positions ------------------------------------------------------ */
#define CONFIG6_WDTCCS_POS 5
#define CONFIG6_WDTCWS_POS 2
#define CONFIG6_WDTE_POS 0

/* -- Field masks ---------------------------------------------------------- */
#define CONFIG6_WDTCCS_MASK 0xE0u /**< WDTCCS field [7:5]              */
#define CONFIG6_WDTCWS_MASK 0x1Cu /**< WDTCWS field [4:2]              */
#define CONFIG6_WDTE_MASK 0x03u   /**< WDTE field [1:0]                */

/* -- WDTCCS field values -------------------------------------------------- */
#define CONFIG6_WDTCCS_LFINTOSC (0x00u << CONFIG6_WDTCCS_POS) /**< LFINTOSC 31 kHz          */
#define CONFIG6_WDTCCS_HFINTOSC (0x01u << CONFIG6_WDTCCS_POS) /**< HFINTOSC divided          */
#define CONFIG6_WDTCCS_SOSC (0x04u << CONFIG6_WDTCCS_POS)     /**< SOSC / T1OSC              */
#define CONFIG6_WDTCCS_SW (0x07u << CONFIG6_WDTCCS_POS)       /**< Software controlled       */

/* -- WDTCWS field values -------------------------------------------------- */
#define CONFIG6_WDTCWS_12_5 (0x00u << CONFIG6_WDTCWS_POS) /**< 12.5% window              */
#define CONFIG6_WDTCWS_25 (0x01u << CONFIG6_WDTCWS_POS)   /**< 25% window                */
#define CONFIG6_WDTCWS_37_5 (0x02u << CONFIG6_WDTCWS_POS) /**< 37.5% window              */
#define CONFIG6_WDTCWS_50 (0x03u << CONFIG6_WDTCWS_POS)   /**< 50% window                */
#define CONFIG6_WDTCWS_62_5 (0x04u << CONFIG6_WDTCWS_POS) /**< 62.5% window              */
#define CONFIG6_WDTCWS_75 (0x05u << CONFIG6_WDTCWS_POS)   /**< 75% window                */
#define CONFIG6_WDTCWS_87_5 (0x06u << CONFIG6_WDTCWS_POS) /**< 87.5% window              */
#define CONFIG6_WDTCWS_100 (0x07u << CONFIG6_WDTCWS_POS)  /**< 100% window (no windowing)*/

/* -- WDTE field values ---------------------------------------------------- */
#define CONFIG6_WDTE_OFF 0x00u /**< WDT disabled entirely            */
#define CONFIG6_WDTE_SW 0x01u  /**< WDT software controlled          */
#define CONFIG6_WDTE_RUN 0x02u /**< WDT enabled in Run; off in Sleep */
#define CONFIG6_WDTE_ON 0x03u  /**< WDT always enabled               */

/* ============================================================================
 *  CONFIG7 - Storage Area Flash / Boot Block Size
 * ============================================================================
 *  Address: 0x300006   (8-bit)
 *
 *    Bit 7   - SAFEN    Storage Area Flash (SAF) Enable
 *              1 = SAF is enabled (portion of program memory for data storage)
 *              0 = SAF is disabled
 *
 *    Bit 6:3 - Reserved (read as 1)
 *
 *    Bit 2:0 - BBSIZE[2:0]  Boot Block Size Selection
 *              000 = 512 words  (1024 bytes)
 *              001 = 1024 words (2048 bytes)
 *              010 = 2048 words (4096 bytes)
 *              011 = 4096 words (8192 bytes)
 *              100 = 8192 words (16384 bytes)
 *              101 = 16384 words (32768 bytes)
 *              110 = 32768 words (65536 bytes)
 *              111 = Reserved
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG7_SAFEN_POS 7
#define CONFIG7_BBSIZE_POS 0

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG7_SAFEN (1u << CONFIG7_SAFEN_POS) /**< 0x80 - 1=SAF enabled              */
#define CONFIG7_BBSIZE_MASK 0x07u               /**< Boot block size field [2:0]      */

/* -- BBSIZE field values -------------------------------------------------- */
#define CONFIG7_BBSIZE_512W 0x00u   /**< 512 words (1 KB)                 */
#define CONFIG7_BBSIZE_1024W 0x01u  /**< 1024 words (2 KB)                */
#define CONFIG7_BBSIZE_2048W 0x02u  /**< 2048 words (4 KB)                */
#define CONFIG7_BBSIZE_4096W 0x03u  /**< 4096 words (8 KB)                */
#define CONFIG7_BBSIZE_8192W 0x04u  /**< 8192 words (16 KB)               */
#define CONFIG7_BBSIZE_16384W 0x05u /**< 16384 words (32 KB)              */
#define CONFIG7_BBSIZE_32768W 0x06u /**< 32768 words (64 KB)              */

/* ============================================================================
 *  CONFIG8 - Boot Partition Configuration
 * ============================================================================
 *  Address: 0x300007   (8-bit)
 *
 *    Bit 7:7 - Reserved (read as 1)
 *
 *    Bit 6   - BPEN     Boot Partition Enable
 *              1 = Boot and application partitions are separately protected
 *              0 = No boot/application partition separation
 *
 *    Bit 5   - BBEN     Boot Block Enable
 *              1 = Boot block is enabled at address 0x000000
 *              0 = Boot block disabled; entire flash is application space
 *
 *    Bit 4:0 - Reserved (read as 1)
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG8_BPEN_POS 6
#define CONFIG8_BBEN_POS 5

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG8_BPEN (1u << CONFIG8_BPEN_POS) /**< 0x40 - 1=boot partition enabled   */
#define CONFIG8_BBEN (1u << CONFIG8_BBEN_POS) /**< 0x20 - 1=boot block enabled       */

/* ============================================================================
 *  CONFIG9 - Write Protection
 * ============================================================================
 *  Address: 0x300008   (8-bit)
 *
 *  Write protection bits for various memory regions.  When a bit is set
 *  (1), the corresponding region is write-protected and cannot be
 *  modified by self-write operations (table writes).
 *
 *    Bit 7:6 - Reserved (read as 1)
 *
 *    Bit 5   - WRTD     Data EEPROM Write Protect
 *              1 = Data EEPROM is write-protected
 *              0 = Data EEPROM is writable
 *
 *    Bit 4   - WRTC     Configuration Register Write Protect
 *              1 = Configuration words are write-protected
 *              0 = Configuration words can be written by self-write
 *
 *    Bit 3   - WRTB     Boot Block Write Protect
 *              1 = Boot block is write-protected
 *              0 = Boot block is writable (if enabled)
 *
 *    Bit 2   - WRTSAF   Storage Area Flash Write Protect
 *              1 = SAF region is write-protected
 *              0 = SAF region is writable (if enabled)
 *
 *    Bit 1   - WRTAPP   Application Block Write Protect
 *              1 = Application program memory is write-protected
 *              0 = Application program memory is writable
 *
 *    Bit 0   - Reserved (read as 1)
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG9_WRTD_POS 5
#define CONFIG9_WRTC_POS 4
#define CONFIG9_WRTB_POS 3
#define CONFIG9_WRTSAF_POS 2
#define CONFIG9_WRTAPP_POS 1

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG9_WRTD (1u << CONFIG9_WRTD_POS)     /**< 0x20 - 1=EEPROM write-protected   */
#define CONFIG9_WRTC (1u << CONFIG9_WRTC_POS)     /**< 0x10 - 1=config write-protected   */
#define CONFIG9_WRTB (1u << CONFIG9_WRTB_POS)     /**< 0x08 - 1=boot block write-protect */
#define CONFIG9_WRTSAF (1u << CONFIG9_WRTSAF_POS) /**< 0x04 - 1=SAF write-protected      */
#define CONFIG9_WRTAPP (1u << CONFIG9_WRTAPP_POS) /**< 0x02 - 1=app block write-protect  */

/* ============================================================================
 *  CONFIG10 - Code Protection / Open-Drain / Boot Pin
 * ============================================================================
 *  Address: 0x300009   (8-bit)
 *
 *    Bit 7   - Reserved (read as 1)
 *
 *    Bit 6   - ODCON    Open-Drain Control for CLKOUT/SDA/SCL
 *              1 = Open-drain output enabled on applicable pins
 *              0 = Standard push-pull output
 *
 *    Bit 5:4 - Reserved (read as 1)
 *
 *    Bit 3:2 - BOOTPINSEL[1:0]  Boot Pin Selection
 *              Selects which pin is used to force boot-mode entry.
 *              00 = RA4 (default)
 *              01 = RA5
 *              10 = RA6
 *              11 = RA7
 *
 *    Bit 1   - Reserved (read as 1)
 *
 *    Bit 0   - CP       User Program Flash Code Protection
 *              1 = User flash is NOT code-protected (readable)
 *              0 = User flash is code-protected (external reads blocked)
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG10_ODCON_POS 6
#define CONFIG10_BOOTPINSEL_POS 2
#define CONFIG10_CP_POS 0

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG10_ODCON (1u << CONFIG10_ODCON_POS) /**< 0x40 - 1=open-drain enabled   */
#define CONFIG10_BOOTPINSEL_MASK 0x0Cu            /**< Boot pin select field [3:2]      */
#define CONFIG10_CP (1u << CONFIG10_CP_POS)       /**< 0x01 - 1=code NOT protected   */

/* -- BOOTPINSEL field values ---------------------------------------------- */
#define CONFIG10_BOOTPINSEL_RA4 (0x00u << CONFIG10_BOOTPINSEL_POS) /**< RA4 as boot pin (default) */
#define CONFIG10_BOOTPINSEL_RA5 (0x01u << CONFIG10_BOOTPINSEL_POS) /**< RA5 as boot pin           */
#define CONFIG10_BOOTPINSEL_RA6 (0x02u << CONFIG10_BOOTPINSEL_POS) /**< RA6 as boot pin           */
#define CONFIG10_BOOTPINSEL_RA7 (0x03u << CONFIG10_BOOTPINSEL_POS) /**< RA7 as boot pin           */

/* ============================================================================
 *  CONFIG11 - Boot / Code Security
 * ============================================================================
 *  Address: 0x30000A   (8-bit)
 *
 *  Controls boot-block and application code security.  These bits
 *  restrict external access to program memory contents via ICSP/JTAG.
 *
 *    Bit 7:4 - Reserved (read as 1)
 *
 *    Bit 3   - BOOTCOE  Boot Block Code-Protect off Enable
 *              1 = Boot block code protection can be disabled by config
 *              0 = Boot block code protection is permanent
 *
 *    Bit 2   - BBEN     Boot Block Code-Protect Enable (mirror)
 *              1 = Boot block code-protected (external reads blocked)
 *              0 = Boot block not code-protected
 *
 *    Bit 1   - SAFSEC   SAF Security
 *              1 = SAF region is secured
 *              0 = SAF region is not secured
 *
 *    Bit 0   - WRTSEC   Write Security
 *              1 = Security write-protect active
 *              0 = Security write-protect inactive
 * ========================================================================= */

/* -- Bit positions -------------------------------------------------------- */
#define CONFIG11_BOOTCOE_POS 3
#define CONFIG11_BBEN_POS 2
#define CONFIG11_SAFSEC_POS 1
#define CONFIG11_WRTSEC_POS 0

/* -- Bit masks ------------------------------------------------------------ */
#define CONFIG11_BOOTCOE (1u << CONFIG11_BOOTCOE_POS) /**< 0x08 - Boot CP off enable       */
#define CONFIG11_BBEN (1u << CONFIG11_BBEN_POS)       /**< 0x04 - Boot block code-protect  */
#define CONFIG11_SAFSEC (1u << CONFIG11_SAFSEC_POS)   /**< 0x02 - SAF security             */
#define CONFIG11_WRTSEC (1u << CONFIG11_WRTSEC_POS)   /**< 0x01 - Write security           */

/* ****************************************************************************
 *  Device Identification Registers  (Read-Only)
 * ****************************************************************************
 *
 *  These registers are factory-programmed during manufacturing and
 *  identify the specific device variant and silicon revision.  They are
 *  located at the top of the configuration address space and can be read
 *  using TBLRD instructions.
 *
 *  REVISIONID (0x3FFFFC, 16-bit):
 *    Bits 15:0 - Silicon revision number.  Incremented with each mask
 *                revision (e.g., A0=0x0000, A1=0x0001, B0=0x0002, ...).
 *
 *  DEVICEID (0x3FFFFE, 16-bit):
 *    Bits 15:0 - Uniquely identifies the device within the PIC18F family.
 *                Known values for Q84 family:
 *                  PIC18F27Q84 = 0x7840
 *                  PIC18F47Q84 = 0x7860
 *                  PIC18F57Q84 = 0x7880
 *                (Exact values may vary; confirm with the latest datasheet.)
 * ***************************************************************************/

#define REVISIONID_ADDR 0x3FFFFCu
#define DEVICEID_ADDR 0x3FFFFEu

/* -- Known Device ID values ----------------------------------------------- */
#define DEVICEID_PIC18F27Q84 0x7840u /**< PIC18F27Q84 (28-pin)             */
#define DEVICEID_PIC18F47Q84 0x7860u /**< PIC18F47Q84 (40/44-pin)          */
#define DEVICEID_PIC18F57Q84 0x7880u /**< PIC18F57Q84 (48-pin)             */

#endif /* PIC18F_Q84_CONFIG_H */
