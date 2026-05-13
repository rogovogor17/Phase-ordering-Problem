#pragma once

#include <string>

#include "data_types.hpp"

namespace phaseordering {

/** Paths to LLVM tools and configuration for the facade. */
struct LLVMConfig {
    std::string llvmBinPath;
    std::string clangPath;
    std::string optPath;
    std::string lliPath;
    int timeoutMs = 30000;

    /** Resolve clang/opt/lli from llvmBinPath if not set explicitly. */
    void resolvePaths();
};

/**
 * Facade that hides low-level LLVM CLI operations.
 *
 * Provides simple methods for compiling, optimizing,
 * verifying and executing LLVM IR. Other classes should
 * not call LLVM tools directly.
 *
 * Automatically discovers versioned tool names
 * (e.g. clang-18, opt-18) via findTool().
 */
class LLVMFacade {
   private:
    LLVMConfig config_;
    bool available_ = false;
    mutable std::string lastError_;

   public:
    explicit LLVMFacade(const LLVMConfig& config = LLVMConfig{});

    /** Compile a C/C++ source file to LLVM IR. Returns empty string on failure.
     */
    std::string compileToIR(const std::string& sourceFile) const;
    /** Apply an optimization sequence to IR. Returns empty string on failure.
     */
    std::string applyPasses(const std::string& ir,
                            const OptSequence& seq) const;
    /** Compile IR to a native executable. Returns path to executable, or empty
     * on failure. */
    std::string compileToNative(const std::string& ir) const;
    /** Execute IR via lli interpreter and return CPU time in ms, or -1 on
     * failure. */
    double runInterpreter(const std::string& ir) const;
    /** Execute a native executable and return CPU time in ms, or -1 on failure.
     */
    double runNative(const std::string& executablePath) const;
    /** Verify that the given IR is well-formed. */
    bool verifyIR(const std::string& ir) const;
    /** Returns true if clang and opt were found. */
    bool isAvailable() const;
    /** Returns the last error message from a failed operation. */
    std::string getLastError() const;
    std::string getClangPath() const;
    std::string getOptPath() const;
    std::string getLliPath() const;
    /** Remove a temporary file created by this facade. */
    void removeTempFile(const std::string& path) const;

   private:
    std::string executeCommand(const std::string& command,
                               int* exitCode = nullptr) const;
    std::string writeTempFile(const std::string& content,
                              const std::string& suffix) const;
    std::string findTool(const std::string& toolName) const;
};

}  // namespace phaseordering
