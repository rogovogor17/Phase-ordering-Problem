#include "evaluator_impl.hpp"
#include "logging.hpp"

namespace phaseordering {

LLVMEvaluator::LLVMEvaluator(const std::string& sourceIR,
                             LLVMFacade llvm,
                             IRAnalyzer analyzer,
                             bool measureRuntime)
    : llvm_(std::move(llvm))
    , analyzer_(std::move(analyzer))
    , sourceIR_(sourceIR)
    , measureRuntime_(measureRuntime) {}

EvaluationResult LLVMEvaluator::evaluate(const OptSequence& seq) {
    EvaluationResult result;
    result.sequence = seq;

    if (sourceIR_.empty()) {
        result.success = false;
        result.errorMessage = "Source IR is empty";
        return result;
    }

    std::string optimizedIR;
    if (seq.empty()) {
        optimizedIR = sourceIR_;
    } else {
        optimizedIR = llvm_.applyPasses(sourceIR_, seq);
    }

    if (optimizedIR.empty()) {
        result.success = false;
        result.errorMessage = "Failed to apply optimization passes";
        if (!llvm_.getLastError().empty()) {
            result.errorMessage += ": " + llvm_.getLastError();
        }
        return result;
    }

    result.optimizedIR = optimizedIR;
    result.metrics = analyzer_.analyze(optimizedIR);

    if (measureRuntime_) {
        double time = llvm_.runInterpreter(optimizedIR);
        if (time >= 0) {
            result.metrics.executionTimeMs = time;
        }
    }

    result.success = true;
    return result;
}

LLVMFacade& LLVMEvaluator::getLLVMFacade() {
    return llvm_;
}

const LLVMFacade& LLVMEvaluator::getLLVMFacade() const {
    return llvm_;
}

} // namespace phaseordering