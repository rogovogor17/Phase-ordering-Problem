#include "evaluator_impl.hpp"

#include "logging.hpp"

namespace phaseordering {

LLVMEvaluator::LLVMEvaluator(const std::string& sourceIR, LLVMFacade llvm,
                             IRAnalyzer analyzer, bool measureRuntime)
    : llvm_(std::move(llvm)),
      analyzer_(std::move(analyzer)),
      sourceIR_(sourceIR),
      measureRuntime_(measureRuntime) {}

EvaluationResult LLVMEvaluator::evaluate(const OptSequence& seq) {
    EvaluationResult result;
    result.sequence = seq;

    // Reject empty source IR — this is never a valid program.
    // Previously the code silently returned sourceIR_ for empty sequences,
    // which meant a broken/empty source would appear to evaluate successfully.
    if (sourceIR_.empty()) {
        result.success = false;
        result.errorMessage = "Source IR is empty — cannot evaluate";
        return result;
    }

    // Validate source IR contains real function definitions.
    // A file that compiled to IR but has no functions (e.g. empty .c file,
    // header-only) must not be treated as a valid baseline.
    if (sourceIR_.find("define ") == std::string::npos) {
        result.success = false;
        result.errorMessage =
            "Source IR contains no function definitions — "
            "the input may be an empty or header-only file";
        return result;
    }

    // All sequences (including empty) go through opt so the IR is fully
    // canonicalized and the resulting metrics are comparable.
    // Previously empty sequences returned raw sourceIR_ directly,
    // bypassing opt. On LLVM 22 (macOS) the raw -O0 IR format differs
    // enough from opt-canonicalized IR that metrics were incomparable,
    // causing the empty sequence to always appear "best".
    std::string optimizedIR = llvm_.applyPasses(sourceIR_, seq);

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

    // If the analyzer returns invalid metrics (totalInstructions < 0)
    // the evaluation is not usable — mark it as failed.
    if (result.metrics.totalInstructions < 0) {
        result.success = false;
        result.errorMessage =
            "IR analysis produced invalid metrics "
            "(no functions or malformed IR after opt)";
        return result;
    }

    // FIX: A result with zero instructions is also invalid —
    // a real program always has at least one instruction.
    if (result.metrics.totalInstructions == 0) {
        result.success = false;
        result.errorMessage =
            "IR analysis found zero instructions — "
            "opt may have eliminated all code (unreachable?)";
        return result;
    }

    if (measureRuntime_) {
        double time = llvm_.runInterpreter(optimizedIR);
        if (time >= 0) {
            result.metrics.executionTimeMs = time;
        }
    }

    result.success = true;
    return result;
}

LLVMFacade& LLVMEvaluator::getLLVMFacade() { return llvm_; }

const LLVMFacade& LLVMEvaluator::getLLVMFacade() const { return llvm_; }

const std::string& LLVMEvaluator::getSourceIR() const { return sourceIR_; }

}  // namespace phaseordering
