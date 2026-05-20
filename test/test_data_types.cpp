#include <gtest/gtest.h>

#include "data_types.hpp"

using namespace phaseordering;

TEST(AlgorithmTypeTest, ToString) {
    EXPECT_EQ(algorithmTypeToString(AlgorithmType::RandomSearch),
              "RandomSearch");
    EXPECT_EQ(algorithmTypeToString(AlgorithmType::HillClimbing),
              "HillClimbing");
    EXPECT_EQ(algorithmTypeToString(AlgorithmType::SimulatedAnnealing),
              "SimulatedAnnealing");
}

TEST(CostModelTypeTest, ToString) {
    EXPECT_EQ(costModelTypeToString(CostModelType::Weighted), "Weighted");
    EXPECT_EQ(costModelTypeToString(CostModelType::InstructionCount),
              "InstructionCount");
    EXPECT_EQ(costModelTypeToString(CostModelType::Runtime), "Runtime");
}

TEST(OptSequenceTest, EmptySequence) {
    OptSequence seq;
    EXPECT_TRUE(seq.empty());
    EXPECT_EQ(seq.size(), 0);
    EXPECT_EQ(seq.toString(), "<empty>");
}

TEST(OptSequenceTest, AddPasses) {
    OptSequence seq;
    seq.add("mem2reg");
    seq.add("instcombine");
    seq.add("simplifycfg");
    EXPECT_FALSE(seq.empty());
    EXPECT_EQ(seq.size(), 3);
    EXPECT_EQ(seq.passes().size(), 3u);
}

TEST(OptSequenceTest, RemoveAt) {
    OptSequence seq;
    seq.add("mem2reg");
    seq.add("instcombine");
    seq.add("simplifycfg");
    seq.removeAt(1);
    EXPECT_EQ(seq.size(), 2);
    EXPECT_EQ(seq.passes()[0], "mem2reg");
    EXPECT_EQ(seq.passes()[1], "simplifycfg");
}

TEST(OptSequenceTest, RemoveAtInvalid) {
    OptSequence seq;
    seq.add("mem2reg");
    seq.removeAt(5);
    EXPECT_EQ(seq.size(), 1);
    seq.removeAt(-1);
    EXPECT_EQ(seq.size(), 1);
}

TEST(OptSequenceTest, ToString) {
    OptSequence seq;
    seq.add("mem2reg");
    seq.add("instcombine");
    EXPECT_EQ(seq.toString(), "mem2reg -> instcombine");
}

TEST(OptSequenceTest, ConstructFromVector) {
    std::vector<std::string> passes = {"a", "b", "c"};
    OptSequence seq(passes);
    EXPECT_EQ(seq.size(), 3);
}

TEST(IRMetricsTest, DefaultValues) {
    IRMetrics metrics;
    EXPECT_EQ(metrics.totalInstructions, 0);
    EXPECT_EQ(metrics.totalBasicBlocks, 0);
    EXPECT_EQ(metrics.totalFunctions, 0);
    EXPECT_DOUBLE_EQ(metrics.avgBasicBlockSize, 0.0);
    EXPECT_EQ(metrics.memoryOps, 0);
    EXPECT_EQ(metrics.branchOps, 0);
    EXPECT_EQ(metrics.callOps, 0);
    EXPECT_EQ(metrics.arithmeticOps, 0);
    EXPECT_DOUBLE_EQ(metrics.executionTimeMs, 0.0);
}

TEST(IRMetricsTest, ToString) {
    IRMetrics metrics;
    metrics.totalInstructions = 100;
    metrics.totalBasicBlocks = 10;
    std::string str = metrics.toString();
    EXPECT_FALSE(str.empty());
    EXPECT_TRUE(str.find("100") != std::string::npos);
}

TEST(SolverConfigTest, DefaultValues) {
    SolverConfig config;
    EXPECT_EQ(config.maxEvaluations, 300);
    EXPECT_EQ(config.sequenceLength, 20);
    EXPECT_FALSE(config.verbose);
    EXPECT_FALSE(config.measureRuntime);
    EXPECT_EQ(config.algorithmType, AlgorithmType::SimulatedAnnealing);
    EXPECT_EQ(config.costModelType, CostModelType::Weighted);
}

TEST(LogConfigTest, DefaultValues) {
    LogConfig config;
    EXPECT_FALSE(config.saveInitialIR);
    EXPECT_FALSE(config.saveFinalIR);
    EXPECT_TRUE(config.printMetricsComparison);
    EXPECT_TRUE(config.printRuntimeComparison);
    EXPECT_TRUE(config.printProgress);
    EXPECT_FALSE(config.printEachEvaluation);
}

TEST(AlgoContextTest, CanSetValues) {
    AlgoContext ctx;
    ctx.maxEvaluations = 50;
    ctx.verbose = true;
    EXPECT_EQ(ctx.maxEvaluations, 50);
    EXPECT_TRUE(ctx.verbose);
    EXPECT_EQ(ctx.evaluator, nullptr);
    EXPECT_EQ(ctx.costModel, nullptr);
}

TEST(AlgoResultTest, DefaultValues) {
    AlgoResult result;
    EXPECT_DOUBLE_EQ(result.bestScore, 0.0);
    EXPECT_EQ(result.evaluationsUsed, 0);
    EXPECT_DOUBLE_EQ(result.improvementRatio, 0.0);
}

TEST(EvaluationResultTest, DefaultValues) {
    EvaluationResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.optimizedIR.empty());
    EXPECT_TRUE(result.errorMessage.empty());
}
