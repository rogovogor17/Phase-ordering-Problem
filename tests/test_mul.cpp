#include <gtest/gtest.h>

#include "utils.hpp"

class MulTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(MulTest, AddParameters) {
    auto [a, b, expected] = GetParam();
    EXPECT_EQ(mul(a, b), expected);
}

INSTANTIATE_TEST_SUITE_P(MulTests, MulTest,
                         ::testing::Values(std::make_tuple(1, 1, 1),
                                           std::make_tuple(2, 3, 6),
                                           std::make_tuple(-1, 1, -1)));
