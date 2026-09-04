#include "test_support.h"

MunitSuite b1_relay_suite(void);
MunitSuite b1_config_suite(void);
MunitSuite b1_sensors_suite(void);
MunitSuite b1_adc_suite(void);
MunitSuite b1_controller_suite(void);

int main(int argc, char* const argv[]) {
    MunitSuite children[] = {
        b1_relay_suite(), b1_config_suite(), b1_sensors_suite(),
        b1_adc_suite(), b1_controller_suite(),
        {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    };
    MunitSuite root = {"", NULL, children, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&root, NULL, argc, argv);
}
