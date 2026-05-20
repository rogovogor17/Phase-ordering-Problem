#include "cost_models.hpp"

#include <algorithm>
#include <cmath>

namespace phaseordering {

WeightedCostModel::WeightedCostModel(double instrW, double memW, double branchW,
                                     double rtW)
    : instructionWeight_(instrW),
      memoryWeight_(memW),
      branchWeight_(branchW),
      runtimeWeight_(rtW) {}

double WeightedCostModel::score(const IRMetrics& metrics,
                                const IRMetrics& baseline) {
    if (metrics.totalInstructions < 0 || baseline.totalInstructions <= 0) {
        return 1e9;
    }

    double score = 0.0;

    score += instructionWeight_ *
             static_cast<double>(metrics.totalInstructions) /
             baseline.totalInstructions;

    if (baseline.memoryOps > 0) {
        score += memoryWeight_ * static_cast<double>(metrics.memoryOps) /
                 baseline.memoryOps;
    } else if (metrics.memoryOps > 0) {
        score += memoryWeight_ * (1.0 + static_cast<double>(metrics.memoryOps));
    }

    if (baseline.branchOps > 0) {
        score += branchWeight_ * static_cast<double>(metrics.branchOps) /
                 baseline.branchOps;
    } else if (metrics.branchOps > 0) {
        score += branchWeight_ * (1.0 + static_cast<double>(metrics.branchOps));
    }

    if (baseline.executionTimeMs > 0 && metrics.executionTimeMs > 0) {
        score +=
            runtimeWeight_ * metrics.executionTimeMs / baseline.executionTimeMs;
    }

    return score;
}

bool WeightedCostModel::isBetter(double a, double b) { return a < b; }

double InstructionCountCostModel::score(const IRMetrics& metrics,
                                        const IRMetrics& baseline) {
    if (metrics.totalInstructions < 0 || baseline.totalInstructions <= 0) {
        return 1e9;
    }
    return static_cast<double>(metrics.totalInstructions) /
           baseline.totalInstructions;
}

bool InstructionCountCostModel::isBetter(double a, double b) { return a < b; }

double RuntimeCostModel::score(const IRMetrics& metrics,
                               const IRMetrics& baseline) {
    if (metrics.totalInstructions < 0) {
        return 1e9;
    }
    if (baseline.executionTimeMs > 0 && metrics.executionTimeMs > 0) {
        return metrics.executionTimeMs / baseline.executionTimeMs;
    }
    if (baseline.totalInstructions > 0) {
        return static_cast<double>(metrics.totalInstructions) /
               baseline.totalInstructions;
    }
    return 1e9;
}

bool RuntimeCostModel::isBetter(double a, double b) { return a < b; }

}  // namespace phaseordering
