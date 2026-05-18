### inspector — host-side CLI that talks to the panel boards over a
### USB-I2C bridge (CH347 via libusb, CP2112 via hidapi).
###
### Dependencies:
###   libusb-1.0 (CH347 bulk transport)
###     macOS:  brew install libusb
###     Linux:  apt install libusb-1.0-0-dev
###   hidapi    (CP2112 HID transport — macOS HID needs hidapi rather than
###             libusb because IOHIDFamily owns the device)
###     macOS:  brew install hidapi
###     Linux:  apt install libhidapi-dev
###
### Links the device-side libcomm sources directly so the CLI uses the same
### message builders / parsers / CRC the boards do. comm_address() — needed
### by the relay_changed / button_changed builders — is stubbed in inspector.c
### (the CLI never calls those, but they have to link).

CC      ?= cc

### Resolve libusb + hidapi include + link flags. Try pkg-config first; fall
### back to a Homebrew prefix probe for each (covers macOS without pkg-config).
PKGCONFIG := $(shell command -v pkg-config 2>/dev/null)
ifeq ($(PKGCONFIG),)
  USB_PREFIX := $(shell brew --prefix libusb 2>/dev/null)
  HID_PREFIX := $(shell brew --prefix hidapi 2>/dev/null)
  ifneq ($(USB_PREFIX),)
    USB_CFLAGS  := -I$(USB_PREFIX)/include
    USB_LDFLAGS := -L$(USB_PREFIX)/lib -lusb-1.0
  else
    USB_LDFLAGS := -lusb-1.0
  endif
  ifneq ($(HID_PREFIX),)
    HID_CFLAGS  := -I$(HID_PREFIX)/include
    HID_LDFLAGS := -L$(HID_PREFIX)/lib -lhidapi
  else
    HID_LDFLAGS := -lhidapi
  endif
else
  USB_CFLAGS  := $(shell $(PKGCONFIG) --cflags libusb-1.0)
  USB_LDFLAGS := $(shell $(PKGCONFIG) --libs   libusb-1.0)
  HID_CFLAGS  := $(shell $(PKGCONFIG) --cflags hidapi)
  HID_LDFLAGS := $(shell $(PKGCONFIG) --libs   hidapi)
endif

LIBCOMM := ../../firmware/libcomm
CFLAGS  := -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 -g \
           -I. -I$(LIBCOMM) $(USB_CFLAGS) $(HID_CFLAGS)
LDFLAGS := $(USB_LDFLAGS) $(HID_LDFLAGS)

SRCS    := inspector.c transport.c ch347.c cp2112.c $(LIBCOMM)/libcomm.c $(LIBCOMM)/crc.c
OBJS    := $(SRCS:.c=.o)

inspector: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f inspector $(OBJS)

.PHONY: clean
