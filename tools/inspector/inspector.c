#include "transport.h"
#include "libcomm.h"
#include "crc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* libcomm.c references comm_address() from comm_build_button_changed and
 * comm_build_channel_changed — the inspector uses comm_build_button_changed
 * to emulate inbound events, so the stub returns a settable sender that
 * cmd_button overrides for the duration of the build call. */
static uint8_t g_fake_sender;

uint8_t comm_address(void) {
    return g_fake_sender;
}

/* If `__attribute__((packed))` on CommMessage gets dropped, host padding
 * silently corrupts every transmitted CRC. Fail the build loudly instead. */
_Static_assert(sizeof(CommMessage) == 9, "CommMessage must be 9 bytes (1 id + 8 union); add packed attribute");

/* ============================================================================
 * Boards
 * ============================================================================
 */

typedef struct {
    const char* name;
    uint8_t addr;
    uint8_t is_sw;
    uint8_t is_button;
} Board;

static const Board g_boards[] = {
    {"main", COMM_ADDRESS_MAIN, 0, 0},
    {"sw", COMM_ADDRESS_SWITCHING, 1, 0},
    {"l", COMM_ADDRESS_BUTTON_BOARD_L, 0, 1},
    {"l2", COMM_ADDRESS_BUTTON_BOARD_L2, 0, 1},
    {"r", COMM_ADDRESS_BUTTON_BOARD_R, 0, 1},
    {"r2", COMM_ADDRESS_BUTTON_BOARD_R2, 0, 1},
};

static const Board* board_lookup(const char* name) {
    for (size_t i = 0; i < sizeof(g_boards) / sizeof(g_boards[0]); i++) {
        if (strcasecmp(g_boards[i].name, name) == 0) {
            return &g_boards[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Integer parsing — accepts decimal, 0xHEX, and 0bBINARY
 * ============================================================================
 */

static int parse_uint(const char* s, unsigned long* out) {
    int base = 0; /* strtoul auto-detect 0x and decimal */
    if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        s += 2;
        base = 2;
    }
    char* end;
    unsigned long v = strtoul(s, &end, base);
    if (*end != '\0' || end == s) {
        return -1;
    }
    *out = v;
    return 0;
}

static int parse_u8(const char* s, uint8_t* out) {
    unsigned long v;
    if (parse_uint(s, &v) < 0 || v > 0xFF) {
        return -1;
    }
    *out = (uint8_t)v;
    return 0;
}

static int parse_u16(const char* s, uint16_t* out) {
    unsigned long v;
    if (parse_uint(s, &v) < 0 || v > 0xFFFF) {
        return -1;
    }
    *out = (uint16_t)v;
    return 0;
}

/* Lines emitted by the current dispatch — `--repeat` uses this to know how
 * many rows to jump back over before re-running. Reset between iterations. */
static int g_lines;

/* In --repeat mode: set when do_read suppressed a CRC error. Tells the
 * repeat loop "don't update the on-screen frame, keep the last good one". */
static int g_repeat;
static int g_crc_suppressed;

/* Top-of-frame spinner state — visible only in --repeat mode. The braille
 * glyph advances each cycle so the user sees the bus is alive; the status
 * icon (◯ ok / ✖ fail) reflects whether the most recent request landed
 * cleanly (no NACK, no USB error, no CRC mismatch). Counters track
 * cumulative success/failure for the whole session so a noisy bus is
 * obvious at a glance instead of disappearing into the suppress-and-hold
 * logic. */
static const char* const SPINNER[] = {
    "\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8", "\xE2\xA0\xBC",
    "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7", "\xE2\xA0\x87", "\xE2\xA0\x8F",
};
#define SPINNER_LEN (sizeof(SPINNER) / sizeof(SPINNER[0]))
#define STATUS_OK_STR   "\xE2\x97\xAF" /* ◯ U+25EF Large Circle */
#define STATUS_FAIL_STR "\xE2\x9C\x96" /* ✖ U+2716 Heavy Multiplication X */
static int g_spinner_idx;
static int g_last_ok = 1;
static uint32_t g_total_requests;
static uint32_t g_ok_requests;
static uint32_t g_baud_khz = 20;

static void eol(void) {
    g_lines++;
}

static void print_spinner(void) {
    printf("%s %s", SPINNER[g_spinner_idx], g_last_ok ? STATUS_OK_STR : STATUS_FAIL_STR);
    if (g_total_requests > 0) {
        /* Successes × 1000 / total → one-decimal percent. uint32 math is
         * fine until counts exceed ~4 M, which won't happen interactively. */
        uint32_t tenths = (g_ok_requests * 1000u + g_total_requests / 2u) / g_total_requests;
        printf(" %lu/%lu (%lu.%lu%%)", (unsigned long)g_ok_requests, (unsigned long)g_total_requests,
               (unsigned long)(tenths / 10u), (unsigned long)(tenths % 10u));
    }
    printf(" @ %s %lukHz", transport_name(), (unsigned long)g_baud_khz);
    /* Clear to end of line (ANSI \033[K) so a previous frame's longer stats
     * text doesn't leave a residual tail when this frame's text is shorter. */
    fputs("\033[K\n", stdout);
    g_spinner_idx = (int)(((unsigned)g_spinner_idx + 1u) % SPINNER_LEN);
    eol();
}

/* ============================================================================
 * Output formatters — "0xNN | 0b NNNN NNNN | NNN" with optional left label
 * ============================================================================
 */

static void print_u8(const char* label, uint8_t v) {
    if (label) {
        printf("%-8s ", label);
    }
    printf("0x%02X | 0b ", v);
    for (int b = 7; b >= 0; b--) {
        putchar((v >> b) & 1 ? '1' : '0');
        if (b == 4) {
            putchar(' ');
        }
    }
    printf(" | %03u\n", v);
    eol();
}

static void print_u16(const char* label, uint16_t v) {
    if (label) {
        printf("%-8s ", label);
    }
    printf("0x%04X | 0b ", v);
    for (int b = 15; b >= 0; b--) {
        putchar((v >> b) & 1 ? '1' : '0');
        if (b == 12 || b == 8 || b == 4) {
            putchar(' ');
        }
    }
    printf(" | %05u\n", v);
    eol();
}

/* ============================================================================
 * Transport wrappers — print "write NACK" / "read NACK" / "CRC error" on
 * the wire-level failure paths. Return -1 on success (caller decodes),
 * 0 if a status was printed, 1 on USB-layer failure.
 * ============================================================================
 */

static int do_write(const Board* b, const CommMessage* msg, uint8_t len) {
    int rc = transport_i2c_write(b->addr, (const uint8_t*)msg, len);
    if (rc == TRANSPORT_WRITE_NACK) {
        puts("write NACK");
        eol();
        g_last_ok = 0;
        return 0;
    }
    if (rc != TRANSPORT_OK) {
        fprintf(stderr, "usb error %d\n", rc);
        g_last_ok = 0;
        return 1;
    }
    g_last_ok = 1;
    return 0;
}

static int do_read(const Board* b, const CommMessage* msg, uint8_t wlen, uint8_t* rdata, uint8_t rlen) {
    int rc = transport_i2c_write_read(b->addr, (const uint8_t*)msg, wlen, rdata, rlen);
    if (rc == TRANSPORT_WRITE_NACK) {
        puts("write NACK");
        eol();
        g_last_ok = 0;
        return 0;
    }
    if (rc == TRANSPORT_READ_NACK) {
        puts("read NACK");
        eol();
        g_last_ok = 0;
        return 0;
    }
    if (rc != TRANSPORT_OK) {
        fprintf(stderr, "usb error %d\n", rc);
        g_last_ok = 0;
        return 1;
    }
    /* Verify the trailing CRC byte. Read responses are [payload...][crc]. */
    if (rlen >= 2 && comm_crc8(rdata, (uint8_t)(rlen - 1)) != rdata[rlen - 1]) {
        g_last_ok = 0;
        if (g_repeat) {
            /* Transient CRC errors are common on a noisy bus — in repeat
             * mode hide them and let the previous good frame stay on
             * screen so the user sees stable live values. */
            g_crc_suppressed = 1;
            return 0;
        }
        puts("CRC error");
        eol();
        /* Show what we tried to validate so a mis-sliced CH347 response is
         * obvious. To dump the USB bytes too, set INSPECTOR_DEBUG=1. */
        fprintf(stderr, "  rdata (%u):", rlen);
        for (uint8_t i = 0; i < rlen; i++) {
            fprintf(stderr, " %02X", rdata[i]);
        }
        fprintf(stderr, "  expected crc8(...) = %02X  got %02X\n",
                comm_crc8(rdata, (uint8_t)(rlen - 1)), rdata[rlen - 1]);
        return 0;
    }
    g_last_ok = 1;
    return -1;
}

/* ============================================================================
 * Command implementations
 * ============================================================================
 */

static int cmd_reset(const Board* b) {
    CommMessage msg;
    uint8_t len = comm_build_reset(&msg);
    return do_write(b, &msg, len);
}

static int cmd_config_read(const Board* b, uint8_t addr) {
    CommMessage msg;
    uint8_t wlen = comm_build_config_read(&msg, addr);
    uint8_t rdata[2];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    print_u8(NULL, rdata[0]);
    return 0;
}

static int cmd_config_write(const Board* b, uint8_t addr, uint8_t val) {
    CommMessage msg;
    uint8_t len = comm_build_config(&msg, addr, val);
    return do_write(b, &msg, len);
}

/* Fake a COMM_BUTTON_CHANGED push to `target` pretending to come from
 * `sender` — used to drive the main board's controller from the inspector
 * without an actual button board present. The libcomm builder pulls the
 * sender address from comm_address(), so we stash it in g_fake_sender for
 * the duration of the build call. */
static int cmd_button_fake(const Board* target, const Board* sender, uint8_t id, uint8_t pressed,
                           CommButtonMode mode) {
    g_fake_sender = sender->addr;
    CommMessage msg;
    uint8_t len = comm_build_button_changed(&msg, id, pressed, mode);
    g_fake_sender = 0;
    return do_write(target, &msg, len);
}

static int cmd_battery(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_battery_read(&msg);
    uint8_t rdata[3];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommBattery batt;
    comm_parse_battery_response(rdata, &batt);
    /* CommBattery.voltage is in millivolts (controller.c format_battery
     * divides by 100 for "XX.YV" display). */
    print_u16("mV", batt.voltage);
    return 0;
}

static int cmd_levels(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_levels_read(&msg);
    uint8_t rdata[3];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommLevels lv;
    comm_parse_levels_response(rdata, &lv);
    print_u8("water", lv.level_0);
    print_u8("fuel", lv.level_1);
    return 0;
}

static int cmd_sensors(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_sensors_read(&msg);
    uint8_t rdata[2];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommSensors s;
    comm_parse_sensors_response(rdata, &s);
    print_u8(NULL, s.sensors);
    return 0;
}

static int cmd_relays_read(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_relay_state_read(&msg);
    uint8_t rdata[3];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommRelayState st;
    comm_parse_relay_state_response(rdata, &st);
    print_u16(NULL, st.relays);
    return 0;
}

static int cmd_relays_write(const Board* b, uint16_t val) {
    CommMessage msg;
    uint8_t len = comm_build_relay_state(&msg, val);
    return do_write(b, &msg, len);
}

static int cmd_channels_read(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_channel_state_read(&msg);
    uint8_t rdata[3];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommChannelState st;
    comm_parse_channel_state_response(rdata, &st);
    print_u16(NULL, st.channels);
    return 0;
}

static int cmd_meter_read(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_level_mode_read(&msg);
    uint8_t rdata[2];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommLevelMode mode;
    comm_parse_level_mode_response(rdata, &mode);
    print_u8("mode0", mode.mode_0);
    print_u8("mode1", mode.mode_1);
    return 0;
}

static int cmd_meter_write(const Board* b, uint8_t m0, uint8_t m1) {
    CommMessage msg;
    uint8_t len = comm_build_level_mode(&msg, (CommMeterMode)m0, (CommMeterMode)m1);
    return do_write(b, &msg, len);
}

static int cmd_buttons(const Board* b) {
    CommMessage msg;
    uint8_t wlen = comm_build_button_state_read(&msg);
    uint8_t rdata[2];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    print_u8(NULL, rdata[0]);
    return 0;
}

static const char* mode_name(uint8_t mode) {
    switch (mode) {
        case COMM_BUTTON_MODE_RELEASE: return "release";
        case COMM_BUTTON_MODE_HOLD:    return "hold";
        case COMM_BUTTON_MODE_CHANGE:  return "change";
        default:                       return "unknown";
    }
}

static int cmd_trigger_read(const Board* b, uint8_t id) {
    CommMessage msg;
    uint8_t wlen = comm_build_button_trigger_read(&msg, id);
    uint8_t rdata[2];
    int r = do_read(b, &msg, wlen, rdata, sizeof(rdata));
    if (r >= 0) {
        return r;
    }
    CommTriggerConfig cfg;
    comm_parse_button_trigger_response(rdata, &cfg);
    print_u8("raw", *(uint8_t*)&cfg);
    printf("mode     %s\n", mode_name(cfg.mode));
    eol();
    printf("time     %u ms\n", comm_button_trigger_time_ms(cfg));
    eol();
    return 0;
}

static CommButtonMode parse_mode_word(const char* s) {
    if (strcasecmp(s, "release") == 0) {
        return COMM_BUTTON_MODE_RELEASE;
    }
    if (strcasecmp(s, "hold") == 0) {
        return COMM_BUTTON_MODE_HOLD;
    }
    if (strcasecmp(s, "change") == 0) {
        return COMM_BUTTON_MODE_CHANGE;
    }
    return COMM_BUTTON_MODE_UNKNOWN;
}

static int cmd_trigger_write(const Board* b, uint8_t id, CommButtonMode mode, uint16_t time_ms) {
    CommTriggerConfig cfg = comm_button_trigger_make(mode, time_ms);
    CommMessage msg;
    uint8_t len = comm_build_button_trigger(&msg, id, cfg);
    return do_write(b, &msg, len);
}

/* ============================================================================
 * CLI
 * ============================================================================
 */

static void usage(void) {
    fputs("usage: inspector [-r|--repeat [ms]] [-b|--baud <khz>] <board> <command> [args]\n"
          "  -r, --repeat [ms]  rerun every <ms> (default 500), refreshing in place;\n"
          "                     transient CRC errors are hidden, last good frame held\n"
          "  -b, --baud <khz>   I2C bus speed in kHz (rounds down to nearest CH347\n"
          "                     stream-protocol step: 20, 100, 400, 750). Default 20.\n"
          "\n"
          "  boards:    main | sw | l | l2 | r | r2\n"
          "\n"
          "  any:       reset\n"
          "             config read  <addr>\n"
          "             config write <addr> <value>\n"
          "             button <sender> <id> <0|1> <release|hold|change>\n"
          "                 — emulate a COMM_BUTTON_CHANGED push from\n"
          "                   <sender> (l|l2|r|r2) to the target board\n"
          "\n"
          "  sw:        battery | voltage\n"
          "             levels\n"
          "             sensors\n"
          "             relays   read | write <hex16>   (commanded target)\n"
          "             channels                        (mux-observed channel voltage)\n"
          "             meter    read | write <m0> <m1> (mode: 0|1|2)\n"
          "\n"
          "  l*, r*:    buttons\n"
          "             trigger read  <id>\n"
          "             trigger write <id> <release|hold|change> <time_ms>\n"
          "\n"
          "integers accept decimal, 0xNN, or 0bNN.\n",
          stderr);
}

static int dispatch(const Board* b, int argc, char** argv) {
    if (argc < 1) {
        usage();
        return 1;
    }
    const char* v = argv[0];

    if (strcasecmp(v, "reset") == 0) {
        return cmd_reset(b);
    }
    if (strcasecmp(v, "config") == 0) {
        if (argc < 3) {
            usage();
            return 1;
        }
        uint8_t addr;
        if (parse_u8(argv[2], &addr) < 0) {
            usage();
            return 1;
        }
        if (strcasecmp(argv[1], "read") == 0) {
            return cmd_config_read(b, addr);
        }
        if (strcasecmp(argv[1], "write") == 0) {
            if (argc < 4) {
                usage();
                return 1;
            }
            uint8_t val;
            if (parse_u8(argv[3], &val) < 0) {
                usage();
                return 1;
            }
            return cmd_config_write(b, addr, val);
        }
    }
    if (strcasecmp(v, "button") == 0) {
        if (argc < 5) {
            usage();
            return 1;
        }
        const Board* sender = board_lookup(argv[1]);
        if (!sender) {
            fprintf(stderr, "unknown sender: %s\n", argv[1]);
            usage();
            return 1;
        }
        uint8_t id;
        if (parse_u8(argv[2], &id) < 0) {
            usage();
            return 1;
        }
        uint8_t pressed;
        if (parse_u8(argv[3], &pressed) < 0 || pressed > 1) {
            usage();
            return 1;
        }
        CommButtonMode mode = parse_mode_word(argv[4]);
        if (mode == COMM_BUTTON_MODE_UNKNOWN) {
            usage();
            return 1;
        }
        return cmd_button_fake(b, sender, id, pressed, mode);
    }
    if (b->is_sw) {
        if (strcasecmp(v, "battery") == 0 || strcasecmp(v, "voltage") == 0) {
            return cmd_battery(b);
        }
        if (strcasecmp(v, "levels") == 0) {
            return cmd_levels(b);
        }
        if (strcasecmp(v, "sensors") == 0) {
            return cmd_sensors(b);
        }
        if (strcasecmp(v, "relays") == 0) {
            if (argc < 2) {
                usage();
                return 1;
            }
            if (strcasecmp(argv[1], "read") == 0) {
                return cmd_relays_read(b);
            }
            if (strcasecmp(argv[1], "write") == 0) {
                if (argc < 3) {
                    usage();
                    return 1;
                }
                uint16_t val;
                if (parse_u16(argv[2], &val) < 0) {
                    usage();
                    return 1;
                }
                return cmd_relays_write(b, val);
            }
        }
        if (strcasecmp(v, "channels") == 0) {
            return cmd_channels_read(b);
        }
        if (strcasecmp(v, "meter") == 0) {
            if (argc < 2) {
                usage();
                return 1;
            }
            if (strcasecmp(argv[1], "read") == 0) {
                return cmd_meter_read(b);
            }
            if (strcasecmp(argv[1], "write") == 0) {
                if (argc < 4) {
                    usage();
                    return 1;
                }
                uint8_t m0, m1;
                if (parse_u8(argv[2], &m0) < 0 || parse_u8(argv[3], &m1) < 0) {
                    usage();
                    return 1;
                }
                return cmd_meter_write(b, m0, m1);
            }
        }
    }
    if (b->is_button) {
        if (strcasecmp(v, "buttons") == 0) {
            return cmd_buttons(b);
        }
        if (strcasecmp(v, "trigger") == 0) {
            if (argc < 2) {
                usage();
                return 1;
            }
            if (strcasecmp(argv[1], "read") == 0) {
                if (argc < 3) {
                    usage();
                    return 1;
                }
                uint8_t id;
                if (parse_u8(argv[2], &id) < 0) {
                    usage();
                    return 1;
                }
                return cmd_trigger_read(b, id);
            }
            if (strcasecmp(argv[1], "write") == 0) {
                if (argc < 5) {
                    usage();
                    return 1;
                }
                uint8_t id;
                if (parse_u8(argv[2], &id) < 0) {
                    usage();
                    return 1;
                }
                CommButtonMode mode = parse_mode_word(argv[3]);
                if (mode == COMM_BUTTON_MODE_UNKNOWN) {
                    usage();
                    return 1;
                }
                uint16_t ms;
                if (parse_u16(argv[4], &ms) < 0) {
                    usage();
                    return 1;
                }
                return cmd_trigger_write(b, id, mode, ms);
            }
        }
    }
    fprintf(stderr, "unknown command for board %s: %s\n", b->name, v);
    usage();
    return 1;
}

int main(int argc, char** argv) {
    int repeat = 0;
    unsigned long interval_ms = 500;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        if (strcmp(argv[argi], "-r") == 0 || strcmp(argv[argi], "--repeat") == 0) {
            repeat = 1;
            argi++;
            /* Optional numeric interval. If the next arg parses as a positive
             * integer we consume it; otherwise it's a board name. None of the
             * board names parse as integers so the lookahead is unambiguous. */
            if (argi < argc) {
                unsigned long v;
                if (parse_uint(argv[argi], &v) == 0 && v > 0) {
                    interval_ms = v;
                    argi++;
                }
            }
        } else if (strcmp(argv[argi], "-b") == 0 || strcmp(argv[argi], "--baud") == 0) {
            argi++;
            if (argi >= argc) {
                fprintf(stderr, "expected <khz> after -b/--baud\n");
                usage();
                return 1;
            }
            unsigned long v;
            if (parse_uint(argv[argi], &v) < 0 || v == 0) {
                fprintf(stderr, "bad baud value: %s\n", argv[argi]);
                usage();
                return 1;
            }
            g_baud_khz = (uint32_t)v;
            argi++;
        } else if (strcmp(argv[argi], "-h") == 0 || strcmp(argv[argi], "--help") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "unknown flag: %s\n", argv[argi]);
            usage();
            return 1;
        }
    }
    if (argc - argi < 2) {
        usage();
        return 1;
    }
    const Board* b = board_lookup(argv[argi]);
    if (!b) {
        fprintf(stderr, "unknown board: %s\n", argv[argi]);
        usage();
        return 1;
    }
    int rc = transport_open(g_baud_khz * 1000u);
    if (rc == TRANSPORT_NOT_FOUND) {
        fprintf(stderr, "no I2C bridge found — tried CH347 (VID 0x1A86, PID 0x55DB)"
                        " and CP2112 (VID 0x10C4, PID 0xEA90)\n");
        return 2;
    }
    if (rc != TRANSPORT_OK) {
        fprintf(stderr, "transport_open failed: %d\n", rc);
        return 2;
    }
    int sub_argc = argc - argi - 1;
    char** sub_argv = argv + argi + 1;

    int r = 0;
    if (!repeat) {
        r = dispatch(b, sub_argc, sub_argv);
    } else {
        g_repeat = 1;
        /* Frame layout in repeat mode:
         *   row 0       — spinner line ("o ⠋" or "x ⠹"), refreshed every iter
         *   row 1..N    — data lines printed by dispatch
         * On a CRC-suppressed iteration only the spinner refreshes; the
         * previous good data block stays on screen. prev_lines tracks the
         * total height of the on-screen frame for the next iter's cursor
         * rewind. */
        int prev_lines = 0;
        int first = 1;
        useconds_t interval_us = (useconds_t)(interval_ms * 1000u);
        for (;;) {
            if (!first && prev_lines > 0) {
                printf("\033[%dA", prev_lines);
            }
            first = 0;
            g_lines = 0;
            g_crc_suppressed = 0;
            print_spinner();
            int spinner_lines = g_lines;
            r = dispatch(b, sub_argc, sub_argv);
            g_total_requests++;
            if (g_last_ok) {
                g_ok_requests++;
            }
            if (g_crc_suppressed) {
                /* Push the cursor past whatever data the previous good
                 * iteration left on screen so the sleeping cursor lands at
                 * the same row as in the steady-state success case. */
                int preserved = prev_lines - spinner_lines;
                if (preserved > 0) {
                    printf("\033[%dB", preserved);
                } else {
                    prev_lines = spinner_lines;
                }
            } else {
                printf("\033[J");
                prev_lines = g_lines;
            }
            fflush(stdout);
            usleep(interval_us);
        }
    }
    transport_close();
    return r;
}
