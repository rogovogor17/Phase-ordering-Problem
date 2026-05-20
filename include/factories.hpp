#pragma once

#include <memory>

#include "data_types.hpp"
#include "interfaces.hpp"

namespace phaseordering {

/**
 * Factory for creating algorithm instances by enum type.
 *
 * Keeps object creation logic outside main().
 *
 * @see IAlgorithm
 */
class AlgorithmFactory {
   public:
    static std::unique_ptr<IAlgorithm> create(AlgorithmType type,
                                              const SolverConfig& config);
};

/**
 * Factory for creating cost model instances by enum type.
 *
 * @see ICostModel
 */
class CostModelFactory {
   public:
    static std::unique_ptr<ICostModel> create(CostModelType type);
};

/**
 * Factory for creating an evaluator with all its dependencies.
 *
 * Hides construction of LLVMFacade, IRAnalyzer,
 * and LLVMEvaluator from user code.
 *
 * @see IEvaluator, LLVMEvaluator
 */
class EvaluatorFactory {
   public:
    static std::unique_ptr<IEvaluator> create(const SolverConfig& config);
};

}  // namespace phaseordering
