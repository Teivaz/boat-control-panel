#include "ch347.h"

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Set INSPECTOR_DEBUG=1 in the environment to dump every USB bulk transfer
 * in hex. Use this to triangulate CH347 protocol mismatches against your
 * specific chip revision / mode without rebuilding. */
static int debug_enabled(void) {
    static int cached = -1;
    if (cached < 0) {
        const char* v = getenv("INSPECTOR_DEBUG");
        cached = (v && v[0] && v[0] != '0') ? 1 : 0;
    }
    return cached;
}

static void hex_dump(const char* tag, const uint8_t* buf, size_t len) {
    if (!debug_enabled()) {
        return;
    }
    fprintf(stderr, "[%s %2zu]", tag, len);
    for (size_t i = 0; i < len; i++) {
        fprintf(stderr, " %02X", buf[i]);
    }
    fputc('\n', stderr);
}

/* ---- CH347T Mode 1 (vendor SPI+I2C+UART) USB identifiers -----------------
 * VID/PID, interface number, and endpoint addresses for the SPI+I2C bulk
 * channel. Adjust if your CH347 is in a different mode. */
#define CH347_VID            0x1A86
#define CH347_PID            0x55DB
#define CH347_INTERFACE      2
#define CH347_EP_OUT         0x06
#define CH347_EP_IN          0x86
#define CH347_BULK_TIMEOUT_MS 500

/* ---- CH341/CH347 I2C stream protocol -------------------------------------
 * Same wire format as CH341 — a frame begins with CMD_I2C_STREAM and chains
 * inline opcodes until STM_END. The slave's per-byte ACK bits come back on
 * the IN endpoint; the device packs them so the high bit of each returned
 * byte is the ACK state (0 = ACK, 1 = NACK) when the corresponding command
 * was a write, and the actual data byte otherwise. */
#define CMD_I2C_STREAM       0xAA
#define STM_STA              0x74          /* start                         */
#define STM_STO              0x75          /* stop                          */
#define STM_OUT(n)           (0x80 | (n))  /* write n bytes; n < 0x40       */
#define STM_IN(n)            (0xC0 | (n))  /* read n bytes; n < 0x40        */
#define STM_END              0x00
/* Speed select: 0x60 + bits. 0x60 = 20 kHz, 0x61 = 100 kHz,
 * 0x62 = 400 kHz, 0x63 = 750 kHz. */
#define STM_SET_SPEED        0x60          /* 20 kHz — slowest, most forgiving */

#define MAX_STREAM_BYTES     63u           /* CH347 stream packet payload   */

static libusb_context*       g_ctx;
static libusb_device_handle* g_dev;

int ch347_open(void) {
    int rc = libusb_init(&g_ctx);
    if (rc != 0) {
        return CH347_USB_ERR;
    }
    g_dev = libusb_open_device_with_vid_pid(g_ctx, CH347_VID, CH347_PID);
    if (!g_dev) {
        libusb_exit(g_ctx);
        g_ctx = NULL;
        return CH347_NOT_FOUND;
    }
    /* Detach a kernel driver only on Linux — macOS doesn't claim CH347 by
     * default, but the call is a no-op on systems where it isn't supported. */
    libusb_set_auto_detach_kernel_driver(g_dev, 1);
    rc = libusb_claim_interface(g_dev, CH347_INTERFACE);
    if (rc != 0) {
        libusb_close(g_dev);
        libusb_exit(g_ctx);
        g_dev = NULL;
        g_ctx = NULL;
        return CH347_USB_ERR;
    }
    /* Configure I2C bus speed once per session. STREAM frame: AA <set> 00. */
    uint8_t init_frame[] = {CMD_I2C_STREAM, STM_SET_SPEED, STM_END};
    int transferred = 0;
    libusb_bulk_transfer(g_dev, CH347_EP_OUT, init_frame, sizeof(init_frame),
                         &transferred, CH347_BULK_TIMEOUT_MS);
    return CH347_OK;
}

void ch347_close(void) {
    if (g_dev) {
        libusb_release_interface(g_dev, CH347_INTERFACE);
        libusb_close(g_dev);
        g_dev = NULL;
    }
    if (g_ctx) {
        libusb_exit(g_ctx);
        g_ctx = NULL;
    }
}

/* Push one CH347 stream frame: bulk-OUT it, then bulk-IN the response.
 * The returned byte count is filled into *in_len. Returns CH347_OK or
 * CH347_USB_ERR. */
static int xfer(const uint8_t* out, size_t out_len,
                uint8_t* in, size_t in_cap, size_t* in_len) {
    if (!g_dev) {
        return CH347_USB_ERR;
    }
    hex_dump("OUT", out, out_len);
    int transferred = 0;
    int rc = libusb_bulk_transfer(g_dev, CH347_EP_OUT, (uint8_t*)out,
                                  (int)out_len, &transferred, CH347_BULK_TIMEOUT_MS);
    if (rc != 0 || transferred != (int)out_len) {
        return CH347_USB_ERR;
    }
    if (in_cap == 0) {
        *in_len = 0;
        return CH347_OK;
    }
    transferred = 0;
    rc = libusb_bulk_transfer(g_dev, CH347_EP_IN, in, (int)in_cap,
                              &transferred, CH347_BULK_TIMEOUT_MS);
    if (rc != 0) {
        return CH347_USB_ERR;
    }
    *in_len = (size_t)transferred;
    hex_dump("IN ", in, *in_len);
    return CH347_OK;
}

int ch347_i2c_write(uint8_t addr7, const uint8_t* data, size_t len) {
    /* Frame: AA STA OUT(1+len) <addr<<1> <data...> STO END */
    if (len + 1u > MAX_STREAM_BYTES) {
        return CH347_BAD_ARG;
    }
    uint8_t frame[MAX_STREAM_BYTES + 8];
    size_t n = 0;
    frame[n++] = CMD_I2C_STREAM;
    frame[n++] = STM_STA;
    frame[n++] = STM_OUT((uint8_t)(len + 1u));
    frame[n++] = (uint8_t)(addr7 << 1);     /* W bit clear */
    memcpy(&frame[n], data, len);
    n += len;
    frame[n++] = STM_STO;
    frame[n++] = STM_END;

    uint8_t resp[MAX_STREAM_BYTES + 8];
    size_t resp_len = 0;
    int rc = xfer(frame, n, resp, sizeof(resp), &resp_len);
    if (rc != CH347_OK) {
        return rc;
    }
    /* For a pure write, the device returns one byte per OUT byte; bit 7 of
     * the *last* response byte is the ACK state of the slave on the final
     * data byte. CH347 collapses ACK reporting into a single status byte
     * with bit 7 indicating NACK on any byte — be conservative and check
     * every returned byte. */
    for (size_t i = 0; i < resp_len; i++) {
        if (resp[i] & 0x80) {
            return CH347_WRITE_NACK;
        }
    }
    return CH347_OK;
}

int ch347_i2c_write_read(uint8_t addr7,
                         const uint8_t* wdata, size_t wlen,
                         uint8_t* rdata, size_t rlen) {
    /* Combined: AA STA OUT(1+wlen) <addr W> <wdata>
     *              STA OUT(1) <addr R>
     *              IN(rlen-1) ... IN(1 with NACK)
     *              STO END
     * CH347 stream caps each IN at 0x3F bytes, and the bus protocol needs
     * the final byte read with NACK. We split rlen as (rlen-1) ACK + 1 NACK
     * by issuing IN with the (n) field treating bit 6 differently — many
     * CH347 variants accept STM_IN with len=0 for "read 1 byte NACK". For
     * portability we just emit one IN(rlen) and let the bridge handle
     * terminal NACK; if your variant requires the split, change here. */
    if (wlen + 2u > MAX_STREAM_BYTES || rlen + 2u > MAX_STREAM_BYTES) {
        return CH347_BAD_ARG;
    }
    uint8_t frame[MAX_STREAM_BYTES + 8];
    size_t n = 0;
    frame[n++] = CMD_I2C_STREAM;
    frame[n++] = STM_STA;
    frame[n++] = STM_OUT((uint8_t)(wlen + 1u));
    frame[n++] = (uint8_t)(addr7 << 1);
    memcpy(&frame[n], wdata, wlen);
    n += wlen;
    frame[n++] = STM_STA;
    frame[n++] = STM_OUT(1);
    frame[n++] = (uint8_t)((addr7 << 1) | 1u);
    frame[n++] = STM_IN((uint8_t)rlen);
    frame[n++] = STM_STO;
    frame[n++] = STM_END;

    uint8_t resp[MAX_STREAM_BYTES + 8];
    size_t resp_len = 0;
    int rc = xfer(frame, n, resp, sizeof(resp), &resp_len);
    if (rc != CH347_OK) {
        return rc;
    }
    /* Response layout: <write-phase ACK bytes...> <read-addr ACK> <data...>
     * The last (1 + wlen) bytes from the write phase are ACK statuses; then
     * one byte for the read-phase addr ACK; then `rlen` data bytes. */
    if (resp_len < wlen + 2u + rlen) {
        return CH347_USB_ERR;
    }
    /* Check write-phase ACKs (covers slave address + every data byte). */
    for (size_t i = 0; i < wlen + 1u; i++) {
        if (resp[i] & 0x80) {
            return CH347_WRITE_NACK;
        }
    }
    /* Check read-phase address ACK. */
    if (resp[wlen + 1u] & 0x80) {
        return CH347_READ_NACK;
    }
    memcpy(rdata, &resp[wlen + 2u], rlen);
    return CH347_OK;
}
