# ── Toolchain (inherited via MAKEFLAGS from the top-level build) ──────────────
MP_CC   ?= xc8-cc
DFP_DIR ?=
CONF    ?= default

# ── Project ───────────────────────────────────────────────────────────────────
MCU    = 18F27Q84
OBJDIR = build/$(CONF)

SRCS = $(wildcard *.c)
OBJS = $(addprefix $(OBJDIR)/,$(SRCS:.c=.p1))

MKDIR = mkdir -p
RM    = rm -f

DEFS ?=

CFLAGS = \
    -mcpu=$(MCU) \
    -mdfp="$(DFP_DIR)/xc8" \
    -memi=wordwrite \
    -O0 \
    -maddrqual=ignore \
    -xassembler-with-cpp \
    -mwarn=-3 \
    -DXPRJ_$(CONF)=$(CONF) \
    -mno-default-config-bits \
    -std=c99 \
    -gdwarf-3 \
    -mstack=compiled:auto:auto:auto \
    $(DEFS)

# ── Targets ───────────────────────────────────────────────────────────────────
# dist just compiles sources to .p1 objects — XC8 .p1 files are pre-linked
# LLVM bitcode and cannot be archived with xc8-ar; the board project links
# them directly.
.PHONY: dist clean

dist: $(OBJS)

$(OBJDIR)/%.p1: %.c Makefile
	@$(MKDIR) $(OBJDIR)
	$(MP_CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) -r build
