#include "test_support.h"

MunitSuite b2_button_suite(void);
MunitSuite b2_led_suite(void);
MunitSuite b2_comm_suite(void);

int main(int argc, char* const argv[]) {
    MunitSuite children[] = {
        b2_button_suite(), b2_led_suite(), b2_comm_suite(),
        {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    };
    MunitSuite root = {"", NULL, children, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&root, NULL, argc, argv);
}
