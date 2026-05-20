#pragma once

#include <string>

#include "data_types.hpp"

namespace phaseordering {

/**
 * Analyzes LLVM IR text and extracts IRMetrics.
 *
 * Parses the IR string using pattern matching to count
 * instructions, basic blocks, functions, and operation types.
 * Does not optimize IR, does not choose passes.
 * Only collects information.
 */
class IRAnalyzer {
   public:
    IRAnalyzer() = default;

    /**
     * Analyze raw LLVM IR text and extract metrics.
     * @param ir LLVM IR as a string.
     * @return Populated IRMetrics.
     */
    IRMetrics analyze(const std::string& ir) const;

   private:
    int countOccurrences(const std::string& text,
                         const std::string& pattern) const;
    int countInstructions(const std::string& ir) const;
    int countBasicBlocks(const std::string& ir) const;
    int countFunctions(const std::string& ir) const;
    int countMemoryOps(const std::string& instrText) const;
    int countBranchOps(const std::string& instrText) const;
    int countCallOps(const std::string& instrText) const;
    int countArithmeticOps(const std::string& instrText) const;
};

}  // namespace phaseordering
