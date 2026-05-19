#include <gtest/gtest.h>

#include <memory>

#include "algorithms.hpp"
#include "cost_models.hpp"
#include "evaluator_impl.hpp"
#include "factories.hpp"
#include "interfaces.hpp"
#include "solver.hpp"

using namespace phaseordering;

class MockEvaluatorForSolver : public IEvaluator {
   public:
    EvaluationResult evaluate(const OptSequence& seq) override {
        EvaluationResult result;
        result.sequence = seq;
        result.metrics.totalInstructions = 800;
        result.metrics.totalBasicBlocks = 40;
        result.metrics.totalFunctions = 5;
        result.metrics.memoryOps = 150;
        result.metrics.branchOps = 80;
        result.metrics.callOps = 20;
        result.metrics.arithmeticOps = 400;
        result.metrics.executionTimeMs = 30.0;
        result.optimizedIR = "; mock optimized IR";
        result.success = true;
        return result;
    }
};

TEST(AlgorithmFactoryTest, CreateRandomSearch) {
    SolverConfig config;
    config.sequenceLength = 5;
    auto algo = AlgorithmFactory::create(AlgorithmType::RandomSearch, config);
    EXPECT_NE(algo, nullptr);
}

TEST(AlgorithmFactoryTest, CreateHillClimbing) {
    SolverConfig config;
    config.maxEvaluations = 100;
    auto algo = AlgorithmFactory::create(AlgorithmType::HillClimbing, config);
    EXPECT_NE(algo, nullptr);
}

TEST(AlgorithmFactoryTest, CreateSimulatedAnnealing) {
    SolverConfig config;
    auto algo =
        AlgorithmFactory::create(AlgorithmType::SimulatedAnnealing, config);
    EXPECT_NE(algo, nullptr);
}

TEST(CostModelFactoryTest, CreateWeighted) {
    auto model = CostModelFactory::create(CostModelType::Weighted);
    EXPECT_NE(model, nullptr);
}

TEST(CostModelFactoryTest, CreateInstructionCount) {
    auto model = CostModelFactory::create(CostModelType::InstructionCount);
    EXPECT_NE(model, nullptr);
}

TEST(CostModelFactoryTest, CreateRuntime) {
    auto model = CostModelFactory::create(CostModelType::Runtime);
    EXPECT_NE(model, nullptr);
}

TEST(SolverBuilderTest, DefaultConfig) {
    SolverBuilder builder;
    auto solver = builder.build();
    EXPECT_NE(solver, nullptr);
}

TEST(SolverBuilderTest, ChainMethods) {
    auto solver = SolverBuilder()
                      .withAlgorithm(AlgorithmType::SimulatedAnnealing)
                      .withCostModel(CostModelType::InstructionCount)
                      .withMaxEvaluations(50)
                      .withSequenceLength(3)
                      .withVerbose(true)
                      .build();
    EXPECT_NE(solver, nullptr);
}

TEST(SolverBuilderTest, ConfigValues) {
    SolverConfig config;
    config.algorithmType = AlgorithmType::HillClimbing;
    config.costModelType = CostModelType::Weighted;
    config.maxEvaluations = 200;
    config.sequenceLength = 7;
    EXPECT_EQ(config.algorithmType, AlgorithmType::HillClimbing);
    EXPECT_EQ(config.costModelType, CostModelType::Weighted);
    EXPECT_EQ(config.maxEvaluations, 200);
    EXPECT_EQ(config.sequenceLength, 7);
}

TEST(PhaseOrderingSolverTest, ConstructorAndConfig) {
    auto algo = std::make_unique<RandomSearch>(5, 5);
    auto eval = std::make_unique<MockEvaluatorForSolver>();
    auto cost = std::make_unique<WeightedCostModel>();

    SolverConfig config;
    config.maxEvaluations = 100;

    PhaseOrderingSolver solver(std::move(algo), std::move(eval),
                               std::move(cost), config);
    EXPECT_EQ(solver.config().maxEvaluations, 100);
}

TEST(PhaseOrderingSolverTest, EvaluateSequence) {
    auto algo = std::make_unique<RandomSearch>(5, 5);
    auto eval = std::make_unique<MockEvaluatorForSolver>();
    auto cost = std::make_unique<WeightedCostModel>();

    SolverConfig config;
    PhaseOrderingSolver solver(std::move(algo), std::move(eval),
                               std::move(cost), config);

    OptSequence seq;
    seq.add("mem2reg");
    EvaluationResult result = solver.evaluate(seq);
    EXPECT_TRUE(result.success);
}
