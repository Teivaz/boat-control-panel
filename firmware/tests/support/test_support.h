/*
 * test_support.h — helpers shared by every suite.
 *
 * Three recurring needs:
 *   1. Putting the mocked hardware back to a known state between tests.
 *   2. Running the cooperative scheduler forward by simulated milliseconds,
 *      since almost every module registers a periodic task.
 *   3. Building and checking protocol frames without hand-computing CRCs.
 */

#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include "pic_mock.h"
#include "task.h"

#include <stdint.h>

/* Reset the SFR mock and erase the EEPROM, i.e. a factory-fresh device that
 * has just come out of reset.  Suites that want persisted settings write them
 * with pic_eeprom_put() after calling this. */
void test_reset_hardware(void);

/* Advance the 1 ms scheduler tick `ms` times, polling after each so callbacks
 * fire at the same cadence they do on the device (tick in the TMR0 ISR, run in
 * the main loop).  This is how a test waits for a debounce window, an
 * animation frame or a retry. */
void test_advance_ms(TaskController* ctrl, uint16_t ms);

/* Poll without advancing time — drains deferred (run_in_main_loop) work and
 * dispatches whatever is already pending. */
void test_poll(TaskController* ctrl);

/* Number of task slots currently occupied.  Modules that arm one-shot timers
 * are supposed to release the slot again; this is how a test proves they did
 * rather than leaking the table. */
uint8_t test_active_task_count(const TaskController* ctrl);

/* 1 if `id` currently occupies a slot. */
uint8_t test_task_active(const TaskController* ctrl, TaskId id);

/* CRC-8 over `data` — the same routine the firmware uses, re-exposed so a
 * test can state "and the trailing byte is a valid CRC" without depending on
 * the layout of libcomm's builders. */
uint8_t test_crc8(const uint8_t* data, uint8_t len);

/* Assert that `frame` is `len` bytes of `id + payload + valid CRC`. */
#define assert_frame(frame, len, id, ...)                                                                              \
    do {                                                                                                               \
        const uint8_t expect_payload_[] = {__VA_ARGS__};                                                               \
        test_assert_frame(__FILE__, __LINE__, (frame), (len), (id), expect_payload_, sizeof(expect_payload_));         \
    } while (0)

void test_assert_frame(const char* file, int line, const uint8_t* frame, uint8_t len, uint8_t id,
                       const uint8_t* payload, uint8_t payload_len);

#endif /* TEST_SUPPORT_H */
