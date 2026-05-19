#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace phaseordering {

constexpr int kMaxSequenceLength = 30;  // NOTE

/** Type of search algorithm. */
enum class AlgorithmType { RandomSearch, HillClimbing, SimulatedAnnealing };

/** Type of cost model for optimization objective. */
enum class CostModelType { Weighted, InstructionCount, Runtime };

/** Convert AlgorithmType to human-readable string. */
std::string algorithmTypeToString(AlgorithmType type);
/** Convert CostModelType to human-readable string. */
std::string costModelTypeToString(CostModelType type);

/**
 * Measurable characteristics of LLVM IR.
 *
 * Stores counts of various IR elements. Does not decide
 * what is "better" -- that logic belongs to ICostModel.
 */
struct IRMetrics {
    int totalInstructions = 0;
    int totalBasicBlocks = 0;
    int totalFunctions = 0;
    double avgBasicBlockSize = 0.0;
    int maxBlockDepth = 0;
    int memoryOps = 0;
    int branchOps = 0;
    int callOps = 0;
    int arithmeticOps = 0;
    double executionTimeMs = 0.0;

    std::string toString() const;
};

/**
 * Ordered sequence of LLVM optimization passes.
 *
 * Represents the solution to the phase-ordering problem:
 * the goal is to find the OptSequence that produces
 * the best IR metrics after application.
 */
class OptSequence {
   private:
    std::vector<std::string> passes_;

   public:
    OptSequence() = default;
    explicit OptSequence(const std::vector<std::string>& passes);  // NOTE

    /** Append a pass to the end of the sequence. */
    void add(const std::string& pass);
    /** Remove the pass at the given index. */
    void removeAt(int index);
    /** Number of passes in the sequence. */
    int size() const;
    /** True if the sequence contains no passes. */
    bool empty() const;
    /** Read-only access to the underlying pass list. */
    const std::vector<std::string>& passes() const;
    /** Read-write access to the underlying pass list. */
    std::vector<std::string>& passes();
    /** Human-readable representation, e.g. "mem2reg -> instcombine". */
    std::string toString() const;
};

/** Result of evaluating an optimization sequence on LLVM IR. */
struct EvaluationResult {
    OptSequence sequence;
    std::string optimizedIR;
    IRMetrics metrics;
    bool success = false;
    std::string errorMessage;
};

/**
 * Configuration for the solver.
 *
 * Contains all user-level parameters: input files,
 * algorithm and cost model selection, evaluation limits,
 * and LLVM paths.
 */
struct SolverConfig {
    std::string sourceFile;
    std::string inputIR;
    int maxEvaluations = 300;
    int sequenceLength = 20;
    bool verbose = false;
    bool measureRuntime = false;
    bool measureRuntimeNative = false;
    std::string llvmBinPath;
    std::string cflags;
    AlgorithmType algorithmType = AlgorithmType::SimulatedAnnealing;
    CostModelType costModelType = CostModelType::Weighted;
};

/** Configuration for logging and output control. */
struct LogConfig {
    bool saveInitialIR = false;
    bool saveFinalIR = false;
    bool printInitialIR = false;
    bool printFinalIR = false;
    bool printMetricsComparison = true;
    bool printRuntimeComparison = true;
    bool printProgress = true;
    bool printEachEvaluation = false;
    std::string outputDir = ".";
};

/**
 * Context passed to IAlgorithm::run().
 *
 * Contains everything an algorithm needs during the search:
 * baseline metrics, evaluator and cost model references,
 * and iteration limits.
 */
struct AlgoContext {
    OptSequence baselineSequence;
    IRMetrics baselineMetrics;
    class IEvaluator* evaluator = nullptr;
    class ICostModel* costModel = nullptr;
    int maxEvaluations = 0;
    int maxSequenceLength = 20;
    bool verbose = false;
};

/**
 * Result returned by IAlgorithm::run().
 *
 * Contains the best found sequence, its metrics and score,
 * number of evaluations used, and improvement ratio.
 */
struct AlgoResult {
    OptSequence bestSequence;
    IRMetrics bestMetrics;
    double bestScore = 0.0;
    int evaluationsUsed = 0;
    double improvementRatio = 0.0;
    std::string log;
};

}  // namespace phaseordering
