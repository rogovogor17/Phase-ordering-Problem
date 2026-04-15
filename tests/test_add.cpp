#include <gtest/gtest.h>

#include "utils.hpp"

class MathTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(MathTest, AddParameters) {
    auto [a, b, expected] = GetParam();
    EXPECT_EQ(add(a, b), expected);
}

INSTANTIATE_TEST_SUITE_P(AddTests, MathTest,
                         ::testing::Values(std::make_tuple(1, 1, 2),
                                           std::make_tuple(2, 3, 5),
                                           std::make_tuple(-1, 1, 0)));
