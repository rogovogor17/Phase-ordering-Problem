#pragma once

#include "data_types.hpp"
#include "interfaces.hpp"

namespace phaseordering {

/**
 * Weighted combination of IR metric ratios.
 *
 * Computes a weighted sum of metric ratios
 * (optimized / baseline). Lower score is better.
 */
class WeightedCostModel : public ICostModel {
   private:
    double instructionWeight_ = 3.0;
    double memoryWeight_ = 1.5;
    double branchWeight_ = 1.0;
    double runtimeWeight_ = 0.0;

   public:
    WeightedCostModel() = default;
    WeightedCostModel(double instrW, double memW, double branchW, double rtW);

    double score(const IRMetrics& metrics, const IRMetrics& baseline) override;
    bool isBetter(double a, double b) override;
};

/**
 * Cost model that minimizes total instruction count.
 *
 * Score = optimized_instructions / baseline_instructions.
 */
class InstructionCountCostModel : public ICostModel {
   public:
    double score(const IRMetrics& metrics, const IRMetrics& baseline) override;
    bool isBetter(double a, double b) override;
};

/**
 * Cost model that minimizes execution time.
 *
 * Uses lli-measured runtime if available,
 * falls back to instruction count otherwise.
 */
class RuntimeCostModel : public ICostModel {
   public:
    double score(const IRMetrics& metrics, const IRMetrics& baseline) override;
    bool isBetter(double a, double b) override;
};

}  // namespace phaseordering
