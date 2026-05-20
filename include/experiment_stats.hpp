/**
 * @file experiment_stats.hpp
 * @brief Experiment result collection, statistics, and CSV output.
 *
 * Defines ProgramResult (per-run data), ExperimentStats (aggregation),
 * raw CSV output, and matrix CSV output with baseline rows and
 * average/standard-deviation columns for repeated runs.
 */

#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "data_types.hpp"

namespace phaseordering {

struct ProgramResult {
    std::string sourceFile;
    std::string configLabel;
    int repetition = 0;
    bool success = false;

    std::string algorithmName;
    std::string costModelName;
    int maxEvaluations = 0;
    int sequenceLength = 0;

    int baselineInstructions = 0;
    int optimizedInstructions = 0;
    double improvementRatio = 0.0;
    double bestScore = 0.0;

    int baselineMemoryOps = 0;
    int optimizedMemoryOps = 0;
    int baselineBranchOps = 0;
    int optimizedBranchOps = 0;

    double baselineRuntimeMs = 0.0;
    double optimizedRuntimeMs = 0.0;

    int evaluationsUsed = 0;
    double elapsedMs = 0.0;

    std::string bestSequence;
    std::string errorMessage;
};

struct ExperimentStats {
    std::vector<ProgramResult> results;

    int totalRuns = 0;
    int successfulRuns = 0;
    int failedRuns = 0;

    double avgImprovementRatio = 0.0;
    double maxImprovementRatio = 0.0;
    double minImprovementRatio = 0.0;
    double medianImprovementRatio = 0.0;

    double avgElapsedMs = 0.0;
    double avgEvaluationsUsed = 0.0;

    double avgBaselineInstructions = 0.0;
    double avgOptimizedInstructions = 0.0;

    void addResult(const ProgramResult& result);
    void compute();

    void printSummary(std::ostream& out) const;

    void writeRawCSV(const std::string& path) const;
    void writeRawCSV(std::ostream& out) const;

    void writeMatrixCSV(const std::string& path) const;
    void writeMatrixCSV(std::ostream& out) const;
};

}  // namespace phaseordering
