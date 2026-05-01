# ── Toolchain (supplied by build.sh / Docker CMD) ────────────────────────────
MP_CC   ?= xc8-cc
DFP_DIR ?=
CONF    ?= default

# ── Project ───────────────────────────────────────────────────────────────────
TARGET  = board2-buttons
MCU     = 18F27Q84

OBJDIR  = build/$(CONF)/production
DISTDIR = dist/$(CONF)/production

LIBCOMM_DIR  = ../libcomm
LIBCOMM_SRCS = $(wildcard $(LIBCOMM_DIR)/*.c)
LIBCOMM_OBJS = $(patsubst $(LIBCOMM_DIR)/%.c,$(LIBCOMM_DIR)/build/$(CONF)/%.p1,$(LIBCOMM_SRCS))

SRCS = $(wildcard *.c)
OBJS = $(addprefix $(OBJDIR)/,$(SRCS:.c=.p1))

MKDIR = mkdir -p
RM    = rm -f

# ── Flags ─────────────────────────────────────────────────────────────────────
DEFS = \
    -DXPRJ_$(CONF)=$(CONF) \
    -DDEVICE_TYPE_INPUT

CFLAGS = \
    -mcpu=$(MCU) \
    -mdfp="$(DFP_DIR)/xc8" \
    -memi=wordwrite \
    -O0 \
    -fasmfile \
    -maddrqual=ignore \
    -xassembler-with-cpp \
    -mwarn=-3 \
    -Wa,-a \
    $(DEFS) \
    -msummary=-psect,-class,+mem,-hex,-file \
    -ginhx32 \
    -Wl,--data-init \
    -mno-keep-startup \
    -mno-download \
    -mno-default-config-bits \
    -std=c99 \
    -gdwarf-3 \
    -mstack=compiled:auto:auto:auto \
    -MMD \
    -I$(LIBCOMM_DIR)

DEPS = $(OBJS:.p1=.d)

LDFLAGS = \
    -Wl,-Map=$(DISTDIR)/$(TARGET).map \
    -Wl,--defsym=__MPLAB_BUILD=1 \
    -Wl,--memorysummary,$(DISTDIR)/memoryfile.xml

# ── Targets ───────────────────────────────────────────────────────────────────
.PHONY: dist clean libcomm

dist: libcomm $(DISTDIR)/$(TARGET).production.hex

libcomm:
	$(MAKE) -C $(LIBCOMM_DIR) dist DEFS="$(DEFS)"

# XC8 generates the .hex alongside the .elf as a side-effect of linking
$(DISTDIR)/$(TARGET).production.hex: $(OBJS) $(LIBCOMM_OBJS)
	@$(MKDIR) $(DISTDIR)
	$(MP_CC) $(CFLAGS) $(LDFLAGS) -o $(DISTDIR)/$(TARGET).production.elf $(OBJS) $(LIBCOMM_OBJS)

$(OBJDIR)/%.p1: %.c Makefile
	@$(MKDIR) $(OBJDIR)
	$(MP_CC) $(CFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	$(MAKE) -C $(LIBCOMM_DIR) clean
	$(RM) -r build dist
