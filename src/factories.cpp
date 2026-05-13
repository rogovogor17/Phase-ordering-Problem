#include "factories.hpp"

#include "algorithms.hpp"
#include "cost_models.hpp"
#include "evaluator_impl.hpp"
#include "ir_analyzer.hpp"

namespace phaseordering {

namespace {
static const int MAX_REASONABLE_SEQ_LEN = 30;
}

std::unique_ptr<IAlgorithm> AlgorithmFactory::create(
    AlgorithmType type, const SolverConfig& config) {
    int cappedLen = std::min(config.sequenceLength, MAX_REASONABLE_SEQ_LEN);
    switch (type) {
        case AlgorithmType::RandomSearch:
            return std::make_unique<RandomSearch>(cappedLen, cappedLen);
        case AlgorithmType::HillClimbing:
            return std::make_unique<HillClimbing>(50, cappedLen);
        case AlgorithmType::SimulatedAnnealing:
            return std::make_unique<SimulatedAnnealing>(50.0, 0.97, cappedLen);
    }
    return std::make_unique<RandomSearch>(config.sequenceLength, cappedLen);
}

std::unique_ptr<ICostModel> CostModelFactory::create(CostModelType type) {
    switch (type) {
        case CostModelType::Weighted:
            return std::make_unique<WeightedCostModel>();
        case CostModelType::InstructionCount:
            return std::make_unique<InstructionCountCostModel>();
        case CostModelType::Runtime:
            return std::make_unique<RuntimeCostModel>();
    }
    return std::make_unique<WeightedCostModel>();
}

std::unique_ptr<IEvaluator> EvaluatorFactory::create(
    const SolverConfig& config) {
    LLVMConfig llvmConfig;
    llvmConfig.llvmBinPath = config.llvmBinPath;
    LLVMFacade llvm(llvmConfig);

    IRAnalyzer analyzer;

    std::string sourceIR;
    if (!config.inputIR.empty()) {
        sourceIR = config.inputIR;
    } else if (!config.sourceFile.empty()) {
        sourceIR = llvm.compileToIR(config.sourceFile);
        if (sourceIR.empty()) {
            std::string err = llvm.getLastError();
            sourceIR = "; ERROR: " + err + "\n";
        }
    }

    return std::make_unique<LLVMEvaluator>(
        sourceIR, std::move(llvm), std::move(analyzer), config.measureRuntime);
}

}  // namespace phaseordering
