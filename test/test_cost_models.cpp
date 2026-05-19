#include <gtest/gtest.h>

#include "cost_models.hpp"

using namespace phaseordering;

TEST(WeightedCostModelTest, DefaultConstructor) {
    WeightedCostModel model;
    IRMetrics baseline;
    baseline.totalInstructions = 1000;
    baseline.memoryOps = 200;
    baseline.branchOps = 100;
    baseline.executionTimeMs = 0.0;

    IRMetrics same;
    same.totalInstructions = 1000;
    same.memoryOps = 200;
    same.branchOps = 100;
    same.executionTimeMs = 0.0;

    double score = model.score(same, baseline);
    double expected = 3.0 + 1.5 + 1.0;
    EXPECT_DOUBLE_EQ(score, expected);
}

TEST(WeightedCostModelTest, BetterMetrics) {
    WeightedCostModel model;
    IRMetrics baseline;
    baseline.totalInstructions = 1000;
    baseline.memoryOps = 200;
    baseline.branchOps = 100;
    baseline.executionTimeMs = 50.0;

    IRMetrics improved;
    improved.totalInstructions = 500;
    improved.memoryOps = 100;
    improved.branchOps = 50;
    improved.executionTimeMs = 25.0;

    double improvedScore = model.score(improved, baseline);
    double baselineScore = model.score(baseline, baseline);
    EXPECT_TRUE(model.isBetter(improvedScore, baselineScore));
}

TEST(WeightedCostModelTest, IsBetter) {
    WeightedCostModel model;
    EXPECT_TRUE(model.isBetter(0.5, 1.0));
    EXPECT_FALSE(model.isBetter(1.5, 1.0));
    EXPECT_FALSE(model.isBetter(1.0, 1.0));
}

TEST(InstructionCountCostModelTest, ScoreCalculation) {
    InstructionCountCostModel model;
    IRMetrics baseline;
    baseline.totalInstructions = 1000;

    IRMetrics half;
    half.totalInstructions = 500;

    double score = model.score(half, baseline);
    EXPECT_DOUBLE_EQ(score, 0.5);
}

TEST(InstructionCountCostModelTest, IsBetter) {
    InstructionCountCostModel model;
    EXPECT_TRUE(model.isBetter(0.5, 1.0));
    EXPECT_FALSE(model.isBetter(2.0, 1.0));
}

TEST(InstructionCountCostModelTest, ZeroBaseline) {
    InstructionCountCostModel model;
    IRMetrics baseline;
    baseline.totalInstructions = 0;

    IRMetrics other;
    other.totalInstructions = 500;

    double score = model.score(other, baseline);
    EXPECT_GE(score, 1e8);
}

TEST(RuntimeCostModelTest, ScoreCalculation) {
    RuntimeCostModel model;
    IRMetrics baseline;
    baseline.executionTimeMs = 100.0;

    IRMetrics faster;
    faster.executionTimeMs = 50.0;

    double score = model.score(faster, baseline);
    EXPECT_DOUBLE_EQ(score, 0.5);
}

TEST(RuntimeCostModelTest, IsBetter) {
    RuntimeCostModel model;
    EXPECT_TRUE(model.isBetter(0.5, 1.0));
    EXPECT_FALSE(model.isBetter(2.0, 1.0));
}

TEST(RuntimeCostModelTest, ZeroRuntimeFallback) {
    RuntimeCostModel model;
    IRMetrics baseline;
    baseline.executionTimeMs = 0.0;
    baseline.totalInstructions = 1000;

    IRMetrics other;
    other.executionTimeMs = 0.0;
    other.totalInstructions = 500;

    double score = model.score(other, baseline);
    EXPECT_DOUBLE_EQ(score, 0.5);
}
