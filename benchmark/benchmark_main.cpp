#include <fstream>
#include <memory>
#include <sstream>

#include "benchmark/benchmark.h"
#include "cost_models.hpp"
#include "ir_analyzer.hpp"
#include "sequence_ops.hpp"
#include "solver.hpp"

using namespace phaseordering;

static void BM_SimpleArithmeticBaseline(benchmark::State& state) {
    std::string source = R"(
int sum(int n) {
    int result = 0;
    for (int i = 1; i <= n; i++) {
        result += i;
    }
    return result;
}
)";

    for (auto _ : state) {
        IRAnalyzer analyzer;
        IRMetrics metrics = analyzer.analyze(source);
        benchmark::DoNotOptimize(metrics);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SimpleArithmeticBaseline);

static void BM_SequenceGeneration(benchmark::State& state) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    std::mt19937 rng(42);
    int length = static_cast<int>(state.range(0));

    for (auto _ : state) {
        auto seq = gen.generateRandom(length, rng);
        benchmark::DoNotOptimize(seq);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SequenceGeneration)->Arg(5)->Arg(10)->Arg(20);

static void BM_SequenceMutation(benchmark::State& state) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    for (int i = 0; i < 10; ++i) {
        seq.add(registry.randomPass(rng));
    }

    for (auto _ : state) {
        auto result = mut.mutate(seq, rng);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SequenceMutation);

static void BM_CostModelScoring(benchmark::State& state) {
    WeightedCostModel model;
    IRMetrics baseline;
    baseline.totalInstructions = 10000;
    baseline.memoryOps = 2000;
    baseline.branchOps = 1500;
    baseline.executionTimeMs = 100.0;

    IRMetrics target;
    target.totalInstructions = 8000;
    target.memoryOps = 1500;
    target.branchOps = 1200;
    target.executionTimeMs = 80.0;

    for (auto _ : state) {
        double s = model.score(target, baseline);
        benchmark::DoNotOptimize(s);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CostModelScoring);

static void BM_IRAnalyzer(benchmark::State& state) {
    std::string ir = R"(
define i32 @main() {
entry:
  %1 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  %2 = load i32, ptr %1, align 4
  %3 = add i32 %2, 1
  store i32 %3, ptr %1, align 4
  %4 = load i32, ptr %1, align 4
  ret i32 %4
}
)";

    IRAnalyzer analyzer;
    for (auto _ : state) {
        IRMetrics metrics = analyzer.analyze(ir);
        benchmark::DoNotOptimize(metrics);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_IRAnalyzer);

BENCHMARK_MAIN();
