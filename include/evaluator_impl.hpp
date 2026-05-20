#pragma once

#include <memory>
#include <string>

#include "interfaces.hpp"
#include "ir_analyzer.hpp"
#include "llvm_facade.hpp"

namespace phaseordering {

/**
 * Evaluates optimization sequences using real LLVM tools.
 *
 * Takes source IR, applies the given OptSequence via LLVMFacade,
 * then analyzes the result with IRAnalyzer to produce metrics.
 * Optionally measures runtime via the interpreter.
 *
 * @see IEvaluator
 */
class LLVMEvaluator : public IEvaluator {
   private:
    LLVMFacade llvm_;
    IRAnalyzer analyzer_;
    std::string sourceIR_;
    bool measureRuntime_;

   public:
    /**
     * @param sourceIR LLVM IR of the unoptimized program.
     * @param llvm Configured LLVMFacade instance.
     * @param analyzer IRAnalyzer for metrics extraction.
     * @param measureRuntime If true, also measure LLVM IR execution time.
     */
    LLVMEvaluator(const std::string& sourceIR, LLVMFacade llvm,
                  IRAnalyzer analyzer, bool measureRuntime = false);

    EvaluationResult evaluate(const OptSequence& seq) override;

    LLVMFacade& getLLVMFacade();
    const LLVMFacade& getLLVMFacade() const;
    const std::string& getSourceIR() const;
};

}  // namespace phaseordering
