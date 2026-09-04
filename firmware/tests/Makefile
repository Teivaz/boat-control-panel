# Host-side unit tests for the boat-control-panel firmware.
#
# The firmware is PIC18 C, but almost none of it is PIC18-*specific* logic —
# it is protocol framing, state machines, debouncing and bit mapping wrapped
# around a handful of special function registers.  Those registers are mocked
# (mocks/xc.h + mocks/pic_sfr_mock.h), so every module compiles and runs on the
# host under a normal compiler with no simulator involved.
#
#   make            build and run everything
#   make build      build only
#   make libcomm    run one suite (also: alt, board1, board2, board3)
#   make list       show every registered test name
#   make V=1        show the compiler command lines
#   make asan=1     rebuild with AddressSanitizer + UBSan
#   make mocks      regenerate mocks/pic_sfr_mock.h from the XC8 DFP header
#
# A single test: ./build/test_board2 /board2/button/hold_fires_after_delay

FW      := ..
LIBCOMM := $(FW)/libcomm
MUNIT   := $(FW)/munit
B1      := $(FW)/board1-switching
B2      := $(FW)/board2-buttons
B3      := $(FW)/board3-main
U8G2    := $(B3)/u8g2

BUILD := build
CC    ?= cc

# Warnings are dialled to what the firmware can actually satisfy today.  The
# three -Wno- flags below are all consequences of compiling 8-bit code on a
# 64-bit host, not latent defects:
#   pointer-to-int-cast              DMA source/destination addresses are
#                                    16/24-bit registers on the device
#   discarded-qualifiers             libcomm.c passes a const buffer to
#                                    comm_crc8, whose parameter is non-const
#   #pragma-messages                 i2c.c's DS80000870F erratum reminder
WARN := -Wall -Wextra -Wno-unused-parameter \
        -Wno-pointer-to-int-cast \
        -Wno-incompatible-pointer-types-discards-qualifiers \
        -Wno-#pragma-messages

CFLAGS_COMMON := -std=c99 -g -O1 $(WARN) -ffunction-sections -fdata-sections \
                 -Imocks -I$(MUNIT) -I$(LIBCOMM) -Isupport

# board3 vendors a trimmed subset of u8g2, and the trimming assumed XC8's
# linker drops unreferenced functions.  u8g2_buffer.c still *names* the PBM/XBM
# writers whose implementation file was left out, so a host link fails unless
# dead code is stripped the same way.
ifeq ($(shell uname -s),Darwin)
LDFLAGS := -Wl,-dead_strip
else
LDFLAGS := -Wl,--gc-sections
endif

ifdef asan
CFLAGS_COMMON += -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS       += -fsanitize=address,undefined
endif

ifndef V
Q := @
endif

# ── Shared object sets ────────────────────────────────────────────────────
#
# interrupt.c and config_bits.c are deliberately excluded from every binary.
# config_bits.c is nothing but `#pragma config` directives, and interrupt.c is
# a table of `__asm("RESET")` vector stubs — with the interrupt attribute
# erased those become ordinary functions that collide with the real handlers
# in i2c.c.  Neither file contains logic a unit test could interrogate.
# main.c is excluded for the obvious reason.

HARNESS_SRC  := $(MUNIT)/munit.c mocks/pic_mock.c support/test_support.c
LIBCOMM_SRC  := $(LIBCOMM)/crc.c $(LIBCOMM)/libcomm.c $(LIBCOMM)/libcomm_interface.c $(LIBCOMM)/task.c

B1_SRC := $(B1)/adc.c $(B1)/comm.c $(B1)/config.c $(B1)/controller.c \
          $(B1)/i2c_board.c $(B1)/relay_mon.c $(B1)/relay_out.c $(B1)/sensors.c
B2_SRC := $(B2)/button.c $(B2)/comm.c $(B2)/config.c $(B2)/i2c_board.c \
          $(B2)/input.c $(B2)/led_effect.c $(B2)/rgbled.c
B3_SRC := $(B3)/button_fx.c $(B3)/comm.c $(B3)/config.c $(B3)/config_mode.c \
          $(B3)/controller.c $(B3)/display.c $(B3)/display_text.c \
          $(B3)/i2c_board.c $(B3)/i2c_log.c $(B3)/indicator.c \
          $(B3)/nav_lights.c $(B3)/rgbled.c
U8G2_SRC := $(wildcard $(U8G2)/*.c)

# ── Test sources ──────────────────────────────────────────────────────────

T_LIBCOMM := $(wildcard suites/libcomm/*.c)
T_DRIVER  := $(wildcard suites/driver/*.c)
T_B1      := $(wildcard suites/board1/*.c)
T_B2      := $(wildcard suites/board2/*.c)
T_B3      := $(wildcard suites/board3/*.c)

# ── Per-binary definitions ────────────────────────────────────────────────
#
# Each board is its own program on the device and each defines modules with
# the same names (config_init, comm_init, rgbled_set...), so they cannot share
# a test binary.  One binary per board, plus one per I2C driver.

SUITES := libcomm driver alt board1 board2 board3

# Protocol library and dispatcher, over the fake driver.
libcomm_SRC   := $(LIBCOMM_SRC) mocks/i2c_fake.c $(T_LIBCOMM)
libcomm_FLAGS := -DDEVICE_TYPE_MAIN

# The two I2C drivers, tested by the *same* suite source: they implement the
# same header, so anything that passes for one and fails for the other is a
# real behavioural divergence rather than a test artefact.
driver_SRC   := $(LIBCOMM)/crc.c $(LIBCOMM)/task.c $(LIBCOMM)/i2c.c $(T_DRIVER)
driver_FLAGS := -DDEVICE_TYPE_MAIN

alt_SRC   := $(LIBCOMM)/crc.c $(LIBCOMM)/task.c $(LIBCOMM)/alt/i2c.c $(T_DRIVER)
alt_FLAGS := -I$(LIBCOMM)/alt -DDEVICE_TYPE_MAIN -DI2C_DRIVER_ALT=1

board1_SRC   := $(LIBCOMM_SRC) mocks/i2c_fake.c $(B1_SRC) $(T_B1)
board1_FLAGS := -DDEVICE_TYPE_SWITCHING -I$(B1)

board2_SRC   := $(LIBCOMM_SRC) mocks/i2c_fake.c $(B2_SRC) $(T_B2)
board2_FLAGS := -DDEVICE_TYPE_INPUT -I$(B2)

board3_SRC   := $(LIBCOMM_SRC) mocks/i2c_fake.c $(B3_SRC) $(U8G2_SRC) $(T_B3)
board3_FLAGS := -DDEVICE_TYPE_MAIN -I$(B3) -I$(U8G2) -DI2C_LOG_VISIBLE=1

BINS := $(addprefix $(BUILD)/test_,$(SUITES))

# ── Rules ─────────────────────────────────────────────────────────────────

.PHONY: all build run clean list mocks $(SUITES)

all: run

build: $(BINS)

# Object files are namespaced per suite so the same firmware source compiled
# with different -D flags does not collide in the build tree.
define SUITE_template
$(1)_OBJ := $$(patsubst %,$(BUILD)/$(1)/%.o,$$($(1)_SRC) $$(HARNESS_SRC))

$(BUILD)/$(1)/%.o: %
	$$(Q)mkdir -p $$(dir $$@)
	$$(Q)echo "  CC    $(1)  $$<"
	$$(Q)$$(CC) $$($(1)_FLAGS) $$(CFLAGS_COMMON) -MMD -MP -c -o $$@ $$<

$(BUILD)/test_$(1): $$($(1)_OBJ)
	$$(Q)echo "  LINK  $$@"
	$$(Q)$$(CC) $$(LDFLAGS) -o $$@ $$^

$(1): $(BUILD)/test_$(1)
	$$(Q)$$<

-include $$($(1)_OBJ:.o=.d)
endef

$(foreach s,$(SUITES),$(eval $(call SUITE_template,$(s))))

run: $(BINS)
	$(Q)fail=0; for b in $(BINS); do \
	  echo ""; echo "==> $$b"; \
	  $$b || fail=1; \
	done; exit $$fail

list: $(BINS)
	$(Q)for b in $(BINS); do echo "==> $$b"; $$b --list; done

mocks:
	$(Q)python3 tools/gen_pic_mock.py

clean:
	$(Q)rm -rf $(BUILD)
