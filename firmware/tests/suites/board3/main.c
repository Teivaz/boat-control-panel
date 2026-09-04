#include "test_support.h"

MunitSuite b3_nav_suite(void);
MunitSuite b3_button_fx_suite(void);
MunitSuite b3_controller_suite(void);
MunitSuite b3_display_suite(void);

int main(int argc, char* const argv[]) {
    MunitSuite children[] = {
        b3_nav_suite(), b3_button_fx_suite(), b3_controller_suite(), b3_display_suite(),
        {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    };
    MunitSuite root = {"", NULL, children, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&root, NULL, argc, argv);
}
