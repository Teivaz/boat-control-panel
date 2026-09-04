#include "test_support.h"

MunitSuite crc_suite(void);
MunitSuite task_suite(void);
MunitSuite libcomm_suite(void);
MunitSuite interface_suite(void);

int main(int argc, char* const argv[]) {
    MunitSuite children[] = {
        crc_suite(), task_suite(), libcomm_suite(), interface_suite(),
        {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    };
    MunitSuite root = {"", NULL, children, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&root, NULL, argc, argv);
}
