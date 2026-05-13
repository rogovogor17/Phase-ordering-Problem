#pragma once

#include "data_types.hpp"

namespace phaseordering {

/**
 * Strategy interface for search algorithms.
 *
 * Implementations explore the space of OptSequence candidates
 * and return the best found sequence. New algorithms can be
 * added without changing PhaseOrderingSolver.
 *
 * @see RandomSearch, HillClimbing, SimulatedAnnealing
 */
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;

    /**
     * Run the search algorithm.
     * @param ctx Context with evaluator, cost model, and limits.
     * @return Best found sequence and its metrics.
     */
    virtual AlgoResult run(const AlgoContext& ctx) = 0;
};

/**
 * Abstraction for evaluating an optimization sequence.
 *
 * An algorithm asks: "How good is this sequence?"
 * It does not know how LLVM passes are executed,
 * how IR is parsed, or how runtime is measured.
 *
 * @see LLVMEvaluator
 */
class IEvaluator {
public:
    virtual ~IEvaluator() = default;

    /**
     * Apply the given sequence to the source IR and collect metrics.
     * @param seq Optimization sequence to evaluate.
     * @return Evaluation result with metrics and status.
     */
    virtual EvaluationResult evaluate(const OptSequence& seq) = 0;
};

/**
 * Strategy interface for optimization objectives.
 *
 * Encapsulates "what is better" so that algorithms
 * can compare two metric snapshots without knowing
 * the specific scoring formula.
 *
 * @see WeightedCostModel, InstructionCountCostModel, RuntimeCostModel
 */
class ICostModel {
public:
    virtual ~ICostModel() = default;

    /**
     * Calculate a score for the given metrics relative to a baseline.
     * Lower is better.
     * @param metrics Metrics after optimization.
     * @param baseline Baseline (unoptimized) metrics.
     * @return Numeric score (lower = better).
     */
    virtual double score(const IRMetrics& metrics, const IRMetrics& baseline) = 0;

    /**
     * Compare two scores.
     * @return True if score @p a is better than score @p b.
     */
    virtual bool isBetter(double a, double b) = 0;
};

} // namespace phaseordering