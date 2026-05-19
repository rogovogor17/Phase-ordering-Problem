#pragma once

#include <memory>
#include <string>

#include "data_types.hpp"
#include "interfaces.hpp"
#include "logging.hpp"

namespace phaseordering {

/**
 * Main orchestrator of the phase-ordering system.
 *
 * Prepares AlgoContext, runs the selected optimization algorithm,
 * and reports results. Does not know details of LLVM internals
 * and does not implement search logic itself.
 *
 * Pattern role: High-level coordinator.
 *
 * @see SolverBuilder, IAlgorithm, IEvaluator, ICostModel
 */
class PhaseOrderingSolver {
   private:
    std::unique_ptr<IAlgorithm> algorithm_;
    std::unique_ptr<IEvaluator> evaluator_;
    std::unique_ptr<ICostModel> costModel_;
    SolverConfig config_;
    Logger logger_;

   public:
    PhaseOrderingSolver(std::unique_ptr<IAlgorithm> algorithm,
                        std::unique_ptr<IEvaluator> evaluator,
                        std::unique_ptr<ICostModel> costModel,
                        SolverConfig config);

    /** Compile a source file to IR, then optimize. */
    AlgoResult solve(const std::string& sourceFile);
    /** Optimize already-available IR. */
    AlgoResult solveIR(const std::string& ir);
    /** Evaluate a specific sequence on the loaded IR. */
    EvaluationResult evaluate(const OptSequence& seq);
    const SolverConfig& config() const;

    void setLogConfig(const LogConfig& logConfig);
    void openLogFile(const std::string& path);
};

/**
 * Builder pattern for convenient solver configuration.
 *
 * Example:
 * @code
 * auto solver = SolverBuilder()
 *     .withSourceFile("test.cpp")
 *     .withAlgorithm(AlgorithmType::HillClimbing)
 *     .withSequenceLength(10)
 *     .withMaxEvaluations(200)
 *     .build();
 * @endcode
 */
class SolverBuilder {
   private:
    SolverConfig config_;
    LogConfig logConfig_;

   public:
    SolverBuilder();

    SolverBuilder& withSourceFile(const std::string& path);
    SolverBuilder& withIR(const std::string& ir);
    SolverBuilder& withAlgorithm(AlgorithmType type);
    SolverBuilder& withCostModel(CostModelType type);
    SolverBuilder& withMaxEvaluations(int value);
    SolverBuilder& withSequenceLength(int value);
    SolverBuilder& withRuntimeMeasurement(bool enabled);
    SolverBuilder& withRuntimeMeasurementNative(bool enabled);
    SolverBuilder& withVerbose(bool enabled);
    SolverBuilder& withLLVMBinPath(const std::string& path);
    SolverBuilder& withCflags(const std::string& flags);
    SolverBuilder& withLogConfig(const LogConfig& logConfig);

    /** Build and return the configured solver. */
    std::unique_ptr<PhaseOrderingSolver> build();
};

}  // namespace phaseordering
