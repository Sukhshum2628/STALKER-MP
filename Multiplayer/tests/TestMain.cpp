// STALKER-MP — Unit test runner entry point (Sprint-001, §7.14)
//
// GoogleTest (vendored in third_party/googletest, see
// third_party/README.md and docs/Decisions.md D-003/D-005).

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
