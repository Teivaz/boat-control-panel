#include "transport.h"

#include "ch347.h"
#include "cp2112.h"

#include <stddef.h>

/* Backend table — probed in order. First open() that doesn't return
 * TRANSPORT_NOT_FOUND wins; the slot becomes the session's transport. */

typedef struct {
    const char* name;
    int (*open)(uint32_t baud_hz);
    void (*close)(void);
    int (*i2c_write)(uint8_t, const uint8_t*, size_t);
    int (*i2c_write_read)(uint8_t, const uint8_t*, size_t, uint8_t*, size_t);
} Backend;

static const Backend BACKENDS[] = {
    /* CP2112 first — proven cleaner signal integrity in practice on this
     * panel. CH347 is the fallback for setups where only the WCH dongle
     * is available. */
    {"CP2112", cp2112_open, cp2112_close, cp2112_i2c_write, cp2112_i2c_write_read},
    {"CH347", ch347_open, ch347_close, ch347_i2c_write, ch347_i2c_write_read},
};

static const Backend* g_active;

int transport_open(uint32_t baud_hz) {
    for (size_t i = 0; i < sizeof(BACKENDS) / sizeof(BACKENDS[0]); i++) {
        int rc = BACKENDS[i].open(baud_hz);
        if (rc == TRANSPORT_OK) {
            g_active = &BACKENDS[i];
            return TRANSPORT_OK;
        }
        /* Hard USB errors (claim failed, libusb init failed, etc.) stop the
         * probe — they indicate the device IS attached but unusable, and
         * silently falling through to the next backend would mislead the
         * user. NOT_FOUND is the only "try next" signal. */
        if (rc != TRANSPORT_NOT_FOUND) {
            return rc;
        }
    }
    return TRANSPORT_NOT_FOUND;
}

void transport_close(void) {
    if (g_active) {
        g_active->close();
        g_active = NULL;
    }
}

const char* transport_name(void) {
    return g_active ? g_active->name : "(none)";
}

int transport_i2c_write(uint8_t addr7, const uint8_t* data, size_t len) {
    if (!g_active) {
        return TRANSPORT_USB_ERR;
    }
    return g_active->i2c_write(addr7, data, len);
}

int transport_i2c_write_read(uint8_t addr7,
                             const uint8_t* wdata, size_t wlen,
                             uint8_t* rdata, size_t rlen) {
    if (!g_active) {
        return TRANSPORT_USB_ERR;
    }
    return g_active->i2c_write_read(addr7, wdata, wlen, rdata, rlen);
}
