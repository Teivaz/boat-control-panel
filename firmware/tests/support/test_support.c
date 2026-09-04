#include "test_support.h"

#include "crc.h"

#include <string.h>

void test_reset_hardware(void) {
    pic_mock_reset();
    pic_eeprom_erase();
}

void test_poll(TaskController* ctrl) {
    task_controller_poll(ctrl);
}

void test_advance_ms(TaskController* ctrl, uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++) {
        task_controller_tick(ctrl);
        task_controller_poll(ctrl);
    }
}

uint8_t test_active_task_count(const TaskController* ctrl) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        if (ctrl->tasks[i].id != TASK_INVALID_ID) {
            n++;
        }
    }
    return n;
}

uint8_t test_task_active(const TaskController* ctrl, TaskId id) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        if (ctrl->tasks[i].id == id) {
            return 1;
        }
    }
    return 0;
}

uint8_t test_crc8(const uint8_t* data, uint8_t len) {
    return comm_crc8((uint8_t*)data, len);
}

void test_assert_frame(const char* file, int line, const uint8_t* frame, uint8_t len, uint8_t id,
                       const uint8_t* payload, uint8_t payload_len) {
    if (len != (uint8_t)(payload_len + 2u)) {
        munit_errorf_ex(file, line, "frame length %u, expected %u (1 id + %u payload + 1 crc)",
                        (unsigned)len, (unsigned)(payload_len + 2u), (unsigned)payload_len);
    }
    if (frame[0] != id) {
        munit_errorf_ex(file, line, "frame id 0x%02X, expected 0x%02X", frame[0], id);
    }
    for (uint8_t i = 0; i < payload_len; i++) {
        if (frame[1 + i] != payload[i]) {
            munit_errorf_ex(file, line, "payload byte %u is 0x%02X, expected 0x%02X",
                            (unsigned)i, frame[1 + i], payload[i]);
        }
    }
    const uint8_t crc = comm_crc8((uint8_t*)frame, (uint8_t)(len - 1u));
    if (frame[len - 1u] != crc) {
        munit_errorf_ex(file, line, "crc byte 0x%02X, expected 0x%02X", frame[len - 1u], crc);
    }
}
