#include "cp2112.h"

#include <hidapi/hidapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CP2112 HID-class USB-to-I2C bridge — Silicon Labs. Uses hidapi rather
 * than raw libusb because macOS's IOHIDFamily driver owns HID-class
 * interfaces and libusb's auto-detach is a no-op for HID there. hidapi
 * goes through IOHIDManager on macOS, hidraw on Linux. Reference: AN495
 * "CP2112 Interface Specification". */

#define CP2112_VID            0x10C4
#define CP2112_PID            0xEA90
#define CP2112_TIMEOUT_MS     500
#define CP2112_REPORT_SIZE    64    /* maximum HID report size for CP2112 */

/* HID report IDs (see AN495 §3). */
#define REPORT_SET_SMB_CONFIG     0x06
#define REPORT_DATA_READ_REQ      0x10  /* pure read                       */
#define REPORT_DATA_WRITE_READ    0x11  /* combined write-then-read        */
#define REPORT_DATA_READ_FORCE    0x12  /* force-send the read payload     */
#define REPORT_DATA_READ_RESPONSE 0x13  /* IN: chunk of read data          */
#define REPORT_DATA_WRITE         0x14  /* pure write                      */
#define REPORT_XFER_STATUS_REQ    0x15
#define REPORT_XFER_STATUS_RESP   0x16

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

static hid_device* g_dev;

/* hid_write expects the report-id as byte 0 and writes the exact byte
 * count given. CP2112 reports are variable-length per report id, but a
 * common-denominator size of CP2112_REPORT_SIZE works for every report
 * we send and avoids per-id length tables — zero-pad the tail. */
static int send_report(const uint8_t* report, size_t len) {
    if (!g_dev) {
        return TRANSPORT_USB_ERR;
    }
    uint8_t buf[CP2112_REPORT_SIZE];
    if (len > sizeof(buf)) {
        return TRANSPORT_BAD_ARG;
    }
    memset(buf, 0, sizeof(buf));
    memcpy(buf, report, len);
    hex_dump("OUT", buf, len);
    int rc = hid_write(g_dev, buf, sizeof(buf));
    return (rc < 0) ? TRANSPORT_USB_ERR : TRANSPORT_OK;
}

static int recv_report(uint8_t* buf, size_t cap, size_t* actual) {
    if (!g_dev) {
        return TRANSPORT_USB_ERR;
    }
    int rc = hid_read_timeout(g_dev, buf, cap, CP2112_TIMEOUT_MS);
    if (rc <= 0) {
        return TRANSPORT_USB_ERR;
    }
    *actual = (size_t)rc;
    hex_dump("IN ", buf, *actual);
    return TRANSPORT_OK;
}

int cp2112_open(uint32_t baud_hz) {
    if (hid_init() != 0) {
        return TRANSPORT_USB_ERR;
    }
    g_dev = hid_open(CP2112_VID, CP2112_PID, NULL);
    if (!g_dev) {
        hid_exit();
        return TRANSPORT_NOT_FOUND;
    }
    /* SMBus Configuration (Report 0x06): clock (big-endian Hz), default
     * slave addr, auto-send-read, write/read/SCL timeouts (ms), retry count.
     *   byte 0     0x06
     *   byte 1..4  clock Hz (BE)
     *   byte 5     own slave addr (host-mode unused; spec default 0x02)
     *   byte 6     auto-send-read (1 = send read data as soon as ready)
     *   byte 7..8  write timeout ms (BE; 0 = none)
     *   byte 9..10 read timeout ms (BE; 0 = none)
     *   byte 11    SCL low timeout (0/1)
     *   byte 12..13 retry time (BE; 0 = no retries) */
    uint8_t cfg[14] = {0};
    cfg[0] = REPORT_SET_SMB_CONFIG;
    cfg[1] = (uint8_t)((baud_hz >> 24) & 0xFF);
    cfg[2] = (uint8_t)((baud_hz >> 16) & 0xFF);
    cfg[3] = (uint8_t)((baud_hz >> 8) & 0xFF);
    cfg[4] = (uint8_t)(baud_hz & 0xFF);
    cfg[5] = 0x02;             /* placeholder slave addr */
    cfg[6] = 0x00;             /* auto-send-read OFF — we poll explicitly */
    cfg[7] = 0x00; cfg[8] = 0xFA;  /* write timeout 250 ms */
    cfg[9] = 0x00; cfg[10] = 0xFA; /* read  timeout 250 ms */
    cfg[11] = 0x00;            /* SCL low timeout disabled */
    cfg[12] = 0x00; cfg[13] = 0x00; /* no auto-retries */
    int sr = send_report(cfg, sizeof(cfg));
    if (sr != TRANSPORT_OK) {
        cp2112_close();
        return sr;
    }
    return TRANSPORT_OK;
}

void cp2112_close(void) {
    if (g_dev) {
        hid_close(g_dev);
        g_dev = NULL;
    }
    hid_exit();
}

/* Poll Transfer Status Response until status0 != Busy (0x01). status0
 * decode (AN495):
 *   0 idle / 1 busy / 2 complete / 3 completed-with-error
 *   Address-NACK shows up as status0=3 with status1=0x02 (write) or 0x03 (read). */
static int wait_idle(uint8_t* out_status0, uint8_t* out_status1, uint16_t* out_read_len) {
    for (int tries = 0; tries < 50; tries++) {
        uint8_t req[2] = {REPORT_XFER_STATUS_REQ, 0x01};
        int sr = send_report(req, sizeof(req));
        if (sr != TRANSPORT_OK) {
            return sr;
        }
        uint8_t resp[CP2112_REPORT_SIZE];
        size_t got = 0;
        int rr = recv_report(resp, sizeof(resp), &got);
        if (rr != TRANSPORT_OK) {
            return rr;
        }
        if (got < 7 || resp[0] != REPORT_XFER_STATUS_RESP) {
            continue;
        }
        if (resp[1] != 0x01) {
            if (out_status0) {
                *out_status0 = resp[1];
            }
            if (out_status1) {
                *out_status1 = resp[2];
            }
            if (out_read_len) {
                *out_read_len = (uint16_t)((resp[5] << 8) | resp[6]);
            }
            return TRANSPORT_OK;
        }
    }
    return TRANSPORT_USB_ERR;
}

int cp2112_i2c_write(uint8_t addr7, const uint8_t* data, size_t len) {
    /* Report 0x14: byte0 id, byte1 addr (R/W=0), byte2 length, bytes3..n data.
     * Max payload 61 bytes (report-size 64 - header 3). */
    if (data == NULL || len == 0 || len > 61u) {
        return TRANSPORT_BAD_ARG;
    }
    uint8_t req[CP2112_REPORT_SIZE];
    req[0] = REPORT_DATA_WRITE;
    req[1] = (uint8_t)(addr7 << 1);
    req[2] = (uint8_t)len;
    memcpy(&req[3], data, len);
    int sr = send_report(req, 3 + len);
    if (sr != TRANSPORT_OK) {
        return sr;
    }
    uint8_t status0 = 0, status1 = 0;
    int ws = wait_idle(&status0, &status1, NULL);
    if (ws != TRANSPORT_OK) {
        return ws;
    }
    if (status0 == 0x02) {
        return TRANSPORT_OK;
    }
    /* status1 = 0x02 → addr-NACK on write; anything else with status0 != 2 is
     * still a write-side failure for our purposes. */
    return TRANSPORT_WRITE_NACK;
}

int cp2112_i2c_write_read(uint8_t addr7,
                          const uint8_t* wdata, size_t wlen,
                          uint8_t* rdata, size_t rlen) {
    /* Report 0x11 (Data Write Read Request):
     *   byte 0     0x11
     *   byte 1     slave addr (R/W=0)
     *   byte 2..3  read length (BE, ≤ 512)
     *   byte 4     target-address length (write-phase byte count, 1..16)
     *   byte 5..n  target-address bytes
     * Then we poll status, send Force-Send (0x12), and collect the read
     * chunks via Data Read Response (0x13). */
    if (wlen == 0 || wlen > 16u || rlen == 0 || rlen > 512u) {
        return TRANSPORT_BAD_ARG;
    }
    uint8_t req[CP2112_REPORT_SIZE] = {0};
    req[0] = REPORT_DATA_WRITE_READ;
    req[1] = (uint8_t)(addr7 << 1);
    req[2] = (uint8_t)((rlen >> 8) & 0xFF);
    req[3] = (uint8_t)(rlen & 0xFF);
    req[4] = (uint8_t)wlen;
    memcpy(&req[5], wdata, wlen);
    int sr = send_report(req, 5 + wlen);
    if (sr != TRANSPORT_OK) {
        return sr;
    }
    uint8_t status0 = 0, status1 = 0;
    uint16_t got_len = 0;
    int ws = wait_idle(&status0, &status1, &got_len);
    if (ws != TRANSPORT_OK) {
        return ws;
    }
    if (status0 != 0x02) {
        /* status1=0x02 write-addr NACK; 0x03 read-addr NACK (AN495 §3.18). */
        if (status1 == 0x02) {
            return TRANSPORT_WRITE_NACK;
        }
        if (status1 == 0x03) {
            return TRANSPORT_READ_NACK;
        }
        return TRANSPORT_USB_ERR;
    }
    /* Force the device to emit the buffered read payload as one or more
     * 0x13 responses. Loop until we've collected rlen bytes. */
    uint8_t force[3] = {REPORT_DATA_READ_FORCE,
                        (uint8_t)((rlen >> 8) & 0xFF),
                        (uint8_t)(rlen & 0xFF)};
    sr = send_report(force, sizeof(force));
    if (sr != TRANSPORT_OK) {
        return sr;
    }
    size_t collected = 0;
    while (collected < rlen) {
        uint8_t resp[CP2112_REPORT_SIZE];
        size_t got = 0;
        int rr = recv_report(resp, sizeof(resp), &got);
        if (rr != TRANSPORT_OK) {
            return rr;
        }
        if (got < 3 || resp[0] != REPORT_DATA_READ_RESPONSE) {
            continue;
        }
        /* resp[1] = status, resp[2] = chunk length, resp[3..] = data. */
        uint8_t chunk = resp[2];
        if (chunk == 0 || (size_t)chunk > got - 3) {
            continue;
        }
        size_t take = chunk;
        if (collected + take > rlen) {
            take = rlen - collected;
        }
        memcpy(&rdata[collected], &resp[3], take);
        collected += take;
    }
    return TRANSPORT_OK;
}
