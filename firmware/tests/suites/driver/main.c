#include "test_support.h"

MunitSuite i2c_suite(void);

int main(int argc, char* const argv[]) {
    MunitSuite children[] = {
        i2c_suite(),
        {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    };
    MunitSuite root = {"", NULL, children, 1, MUNIT_SUITE_OPTION_NONE};
    return munit_suite_main(&root, NULL, argc, argv);
}
