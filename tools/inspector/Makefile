### inspector — host-side CLI that talks to the panel boards over CH347 USB-I2C
###
### Depends on libusb-1.0.
###   macOS:  brew install libusb
###   Linux:  apt install libusb-1.0-0-dev
###
### Links the device-side libcomm sources directly so the CLI uses the same
### message builders / parsers / CRC the boards do. comm_address() — needed
### by the relay_changed / button_changed builders — is stubbed in inspector.c
### (the CLI never calls those, but they have to link).

CC      ?= cc
PKG     := libusb-1.0

### Resolve libusb include + link flags. Try pkg-config; fall back to a
### Homebrew prefix probe (covers macOS without pkg-config installed). If
### neither works, hand-set USB_CFLAGS / USB_LDFLAGS on the make command line.
PKGCONFIG := $(shell command -v pkg-config 2>/dev/null)
ifeq ($(PKGCONFIG),)
  BREW_PREFIX := $(shell brew --prefix libusb 2>/dev/null)
  ifneq ($(BREW_PREFIX),)
    USB_CFLAGS  := -I$(BREW_PREFIX)/include
    USB_LDFLAGS := -L$(BREW_PREFIX)/lib -lusb-1.0
  else
    USB_CFLAGS  :=
    USB_LDFLAGS := -lusb-1.0
  endif
else
  USB_CFLAGS  := $(shell $(PKGCONFIG) --cflags $(PKG))
  USB_LDFLAGS := $(shell $(PKGCONFIG) --libs $(PKG))
endif

LIBCOMM := ../../firmware/libcomm
CFLAGS  := -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 -g \
           -I. -I$(LIBCOMM) $(USB_CFLAGS)
LDFLAGS := $(USB_LDFLAGS)

SRCS    := inspector.c ch347.c $(LIBCOMM)/libcomm.c $(LIBCOMM)/crc.c
OBJS    := $(SRCS:.c=.o)

inspector: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f inspector $(OBJS)

.PHONY: clean
