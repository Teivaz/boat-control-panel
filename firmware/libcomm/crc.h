#ifndef LIBCOMM_CRC_H
#define LIBCOMM_CRC_H

#include <stdint.h>

/* CRC-8, poly 0x07, init 0x00 (SMBus / SAE-J1850).
 *
 * Used on every on-wire protocol message: writes carry an appended CRC
 * byte covering [id, payload...]; read responses carry one covering the
 * payload only.  Receivers reject any message whose trailing byte doesn't
 * match.
 *
 * Implementation is table-driven: crc8_table is a 256-entry PFM lookup
 * (poly 0x07 pre-shifted) so each byte advances the accumulator in one
 * PFM read rather than an 8-iteration bit loop.  The table is exported so
 * tight per-byte loops (e.g. the unrolled builder finaliser in libcomm.c)
 * can reference it directly without a function call. */

extern const uint8_t crc8_table[256];

uint8_t comm_crc8(const uint8_t* data, uint8_t len);

#endif /* LIBCOMM_CRC_H */
