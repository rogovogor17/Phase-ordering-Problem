#include <gtest/gtest.h>

#include <memory>

#include "algorithms.hpp"
#include "data_types.hpp"
#include "interfaces.hpp"

using namespace phaseordering;

class MockEvaluator : public IEvaluator {
   public:
    int callCount = 0;
    double baseScore_ = 100.0;
    double improvementPerEval_ = 1.0;

    EvaluationResult evaluate(const OptSequence& seq) override {
        callCount++;
        EvaluationResult result;
        result.sequence = seq;
        result.metrics.totalInstructions = 1000 - callCount * 10;
        result.metrics.totalBasicBlocks = 50 - callCount;
        result.metrics.totalFunctions = 5;
        result.metrics.memoryOps = 200 - callCount * 2;
        result.metrics.branchOps = 100 - callCount;
        result.metrics.callOps = 30;
        result.metrics.arithmeticOps = 500 - callCount * 5;
        result.metrics.executionTimeMs = 50.0;
        result.success = true;
        return result;
    }
};

class MockCostModel : public ICostModel {
   public:
    double score(const IRMetrics& metrics, const IRMetrics& baseline) override {
        return static_cast<double>(metrics.totalInstructions) /
               baseline.totalInstructions;
    }

    bool isBetter(double a, double b) override { return a < b; }
};

static AlgoContext makeContext(IEvaluator* eval, ICostModel* cost,
                               int maxEvals = 20) {
    AlgoContext ctx;
    ctx.evaluator = eval;
    ctx.costModel = cost;
    ctx.maxEvaluations = maxEvals;
    ctx.verbose = false;
    ctx.baselineMetrics.totalInstructions = 1000;
    ctx.baselineMetrics.totalBasicBlocks = 50;
    ctx.baselineMetrics.totalFunctions = 5;
    ctx.baselineMetrics.memoryOps = 200;
    ctx.baselineMetrics.branchOps = 100;
    ctx.baselineMetrics.callOps = 30;
    ctx.baselineMetrics.arithmeticOps = 500;
    ctx.baselineMetrics.executionTimeMs = 50.0;
    return ctx;
}

TEST(RandomSearchTest, ReturnsResult) {
    RandomSearch algo(3, 3);
    MockEvaluator eval;
    MockCostModel cost;
    auto ctx = makeContext(&eval, &cost, 10);

    AlgoResult result = algo.run(ctx);
    EXPECT_GT(result.evaluationsUsed, 0);
    EXPECT_NE(result.bestScore, 0.0);
}

TEST(RandomSearchTest, UsesEvaluations) {
    RandomSearch algo(3, 3);
    MockEvaluator eval;
    MockCostModel cost;
    auto ctx = makeContext(&eval, &cost, 10);

    AlgoResult result = algo.run(ctx);
    EXPECT_LE(result.evaluationsUsed, 11);
    EXPECT_GT(result.evaluationsUsed, 0);
}

TEST(HillClimbingTest, ReturnsResult) {
    HillClimbing algo(10, 5);
    MockEvaluator eval;
    MockCostModel cost;
    auto ctx = makeContext(&eval, &cost, 50);

    AlgoResult result = algo.run(ctx);
    EXPECT_GT(result.evaluationsUsed, 0);
}

TEST(HillClimbingTest, FindsImprovement) {
    HillClimbing algo(10, 5);
    MockEvaluator eval;
    MockCostModel cost;
    auto ctx = makeContext(&eval, &cost, 50);

    AlgoResult result = algo.run(ctx);
    EXPECT_FALSE(result.bestSequence.empty() && result.bestScore == 0.0);
}

TEST(SimulatedAnnealingTest, ReturnsResult) {
    SimulatedAnnealing algo(100.0, 0.95, 5);
    MockEvaluator eval;
    MockCostModel cost;
    auto ctx = makeContext(&eval, &cost, 50);

    AlgoResult result = algo.run(ctx);
    EXPECT_GT(result.evaluationsUsed, 0);
}

TEST(SimulatedAnnealingTest, UsesEvaluations) {
    SimulatedAnnealing algo(100.0, 0.95, 5);
    MockEvaluator eval;
    MockCostModel cost;
    auto ctx = makeContext(&eval, &cost, 30);

    AlgoResult result = algo.run(ctx);
    EXPECT_LE(result.evaluationsUsed, 30);
}
