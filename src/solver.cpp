#include "solver.hpp"

#include <filesystem>
#include <sstream>

#include "evaluator_impl.hpp"
#include "factories.hpp"
#include "ir_analyzer.hpp"
#include "llvm_facade.hpp"
#include "sequence_ops.hpp"

namespace phaseordering {

PhaseOrderingSolver::PhaseOrderingSolver(std::unique_ptr<IAlgorithm> algorithm,
                                         std::unique_ptr<IEvaluator> evaluator,
                                         std::unique_ptr<ICostModel> costModel,
                                         SolverConfig config)
    : algorithm_(std::move(algorithm)),
      evaluator_(std::move(evaluator)),
      costModel_(std::move(costModel)),
      config_(config),
      logger_(LogConfig{}) {}

AlgoResult PhaseOrderingSolver::solve(const std::string& sourceFile) {
    config_.sourceFile = sourceFile;

    LLVMEvaluator* llvmEval = dynamic_cast<LLVMEvaluator*>(evaluator_.get());
    if (!llvmEval) {
        logger_.error("Source file compilation requires LLVM evaluator");
        AlgoResult result;
        result.log = "LLVM evaluator required for source file compilation";
        return result;
    }

    if (!llvmEval->getLLVMFacade().isAvailable()) {
        logger_.error(
            "LLVM tools (clang/opt) not found. Install LLVM or specify "
            "--llvm-path");
        AlgoResult result;
        result.log = "LLVM tools not found";
        return result;
    }

    std::string ir;

    if (!config_.inputIR.empty()) {
        ir = config_.inputIR;
    } else {
        const std::string& cachedIR = llvmEval->getSourceIR();
        if (!cachedIR.empty() &&
            cachedIR.find("define ") != std::string::npos) {
            logger_.info("Using IR already compiled by evaluator");
            ir = cachedIR;
        } else {
            logger_.info("Compiling source file: " + sourceFile);
            logger_.info("Using clang: " +
                         llvmEval->getLLVMFacade().getClangPath());
            logger_.info("Using opt: " +
                         llvmEval->getLLVMFacade().getOptPath());

            ir = llvmEval->getLLVMFacade().compileToIR(sourceFile);
            if (ir.empty()) {
                std::string err = llvmEval->getLLVMFacade().getLastError();
                logger_.error("Failed to compile source file to IR: " + err);
                AlgoResult result;
                result.log = "Failed to compile source file: " + err;
                return result;
            }

            logger_.info("Successfully compiled to IR (" +
                         std::to_string(ir.size()) + " bytes)");
        }
    }

    config_.inputIR = ir;

    return solveIR(ir);
}

AlgoResult PhaseOrderingSolver::solveIR(const std::string& ir) {
    if (ir.empty()) {
        logger_.error("Input IR is empty — cannot optimize");
        AlgoResult fail;
        fail.log = "Input IR is empty";
        return fail;
    }
    if (ir.find("define ") == std::string::npos) {
        logger_.error(
            "Input IR contains no function definitions. "
            "This may be an empty file, a header-only file, or a compilation "
            "error.");
        AlgoResult fail;
        fail.log =
            "Input IR has no function definitions — cannot optimize an empty "
            "program";
        return fail;
    }

    logger_.info("Starting optimization with " +
                 algorithmTypeToString(config_.algorithmType) + " algorithm");

    // Establish baseline: run opt with no passes to get a canonicalized
    // starting point. FIX: Previously we evaluated an empty OptSequence here,
    // which returned raw IR on the old code (before evaluator fix). Now it goes
    // through opt correctly.
    EvaluationResult baselineEval = evaluator_->evaluate(OptSequence());
    IRMetrics baselineMetrics;

    if (baselineEval.success && baselineEval.metrics.totalInstructions > 0) {
        baselineMetrics = baselineEval.metrics;
        logger_.info("Baseline metrics collected (" +
                     std::to_string(baselineMetrics.totalInstructions) +
                     " instructions)");
    } else {
        // Fall back to static text analysis if opt baseline fails
        logger_.info("Baseline evaluation via LLVM failed (" +
                     baselineEval.errorMessage +
                     "), using IR text analysis for baseline");
        IRAnalyzer analyzer;
        baselineMetrics = analyzer.analyze(ir);
        if (baselineMetrics.totalInstructions <= 0) {
            logger_.error(
                "IR text analysis also failed to produce valid baseline "
                "metrics. "
                "The input IR may be malformed.");
            AlgoResult fail;
            fail.log = "Could not establish baseline metrics from input IR";
            return fail;
        }
    }

    AlgoContext ctx;
    ctx.baselineSequence = OptSequence();
    ctx.baselineMetrics = baselineMetrics;
    ctx.evaluator = evaluator_.get();
    ctx.costModel = costModel_.get();
    ctx.maxEvaluations = config_.maxEvaluations;
    ctx.maxSequenceLength = config_.sequenceLength;
    ctx.verbose = config_.verbose;

    logger_.progress("Running optimization algorithm...");
    AlgoResult result = algorithm_->run(ctx);

    if (result.bestSequence.empty()) {
        logger_.error(
            "Optimization failed: the algorithm did not find any valid "
            "sequence.");
        logger_.error(
            "This often means all candidate pass sequences failed in opt, "
            "likely due to a pass name incompatibility with the installed LLVM "
            "version.");
        logger_.error("Algorithm log:\n" + result.log);
        return result;
    }

    if (baselineMetrics.totalInstructions > 0 &&
        result.bestMetrics.totalInstructions > 0) {
        result.improvementRatio =
            static_cast<double>(baselineMetrics.totalInstructions -
                                result.bestMetrics.totalInstructions) /
            baselineMetrics.totalInstructions;
    }

    if (baselineEval.success) {
        logger_.printMetricsComparison(baselineMetrics, result.bestMetrics);
    }

    if (config_.measureRuntime && baselineEval.metrics.executionTimeMs > 0 &&
        result.bestMetrics.executionTimeMs > 0) {
        logger_.printRuntimeComparison(baselineEval.metrics.executionTimeMs,
                                       result.bestMetrics.executionTimeMs);
    }

    EvaluationResult bestEval = evaluator_->evaluate(result.bestSequence);
    if (bestEval.success) {
        logger_.printFinalIR(bestEval.optimizedIR);
    } else {
        logger_.info("Could not re-evaluate best sequence: " +
                     bestEval.errorMessage);
    }

    logger_.printAlgoResult(result);

    if (config_.measureRuntimeNative) {
        logger_.info("Measuring native execution time...");
        LLVMEvaluator* llvmEval =
            dynamic_cast<LLVMEvaluator*>(evaluator_.get());
        if (llvmEval && baselineEval.success && bestEval.success) {
            std::string baselineExe = llvmEval->getLLVMFacade().compileToNative(
                baselineEval.optimizedIR);
            std::string optimizedExe =
                llvmEval->getLLVMFacade().compileToNative(bestEval.optimizedIR);

            if (!baselineExe.empty() && !optimizedExe.empty()) {
                double nativeBaseline =
                    llvmEval->getLLVMFacade().runNative(baselineExe);
                double nativeOptimized =
                    llvmEval->getLLVMFacade().runNative(optimizedExe);

                if (nativeBaseline > 0 && nativeOptimized > 0) {
                    std::cout << "\n=== Native Execution Comparison ==="
                              << std::endl;
                    logger_.printRuntimeComparison(nativeBaseline,
                                                   nativeOptimized);
                } else {
                    logger_.info(
                        "Native execution failed or returned zero time");
                }

                llvmEval->getLLVMFacade().removeTempFile(baselineExe);
                llvmEval->getLLVMFacade().removeTempFile(optimizedExe);
            } else {
                logger_.info(
                    "Native compilation failed. clang may not be available.");
                if (!baselineExe.empty())
                    llvmEval->getLLVMFacade().removeTempFile(baselineExe);
                if (!optimizedExe.empty())
                    llvmEval->getLLVMFacade().removeTempFile(optimizedExe);
            }
        } else {
            logger_.info("Native runtime comparison requires LLVM evaluator");
        }
    }

    if (config_.verbose && !result.log.empty()) {
        std::cout << "\n--- Full Algorithm Log ---\n"
                  << result.log << std::endl;
        if (logger_.isLoggingToFile()) {
            logger_.writeToLogFile("\n--- Full Algorithm Log ---\n" +
                                   result.log + "\n");
        }
    }

    return result;
}

EvaluationResult PhaseOrderingSolver::evaluate(const OptSequence& seq) {
    return evaluator_->evaluate(seq);
}

const SolverConfig& PhaseOrderingSolver::config() const { return config_; }

void PhaseOrderingSolver::setLogConfig(const LogConfig& logConfig) {
    logger_.setConfig(logConfig);
}

void PhaseOrderingSolver::openLogFile(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path p(path);
    fs::path dir = p.parent_path();
    if (!dir.empty()) {
        std::error_code ec;
        fs::create_directories(dir, ec);
    }
    logger_.openLogFile(path);
}

SolverBuilder::SolverBuilder() {
    config_.algorithmType = AlgorithmType::SimulatedAnnealing;
    config_.costModelType = CostModelType::Weighted;
    config_.maxEvaluations = 300;
    config_.sequenceLength = 20;
    config_.verbose = false;
    config_.measureRuntime = false;
}

SolverBuilder& SolverBuilder::withSourceFile(const std::string& path) {
    config_.sourceFile = path;
    return *this;
}

SolverBuilder& SolverBuilder::withIR(const std::string& ir) {
    config_.inputIR = ir;
    return *this;
}

SolverBuilder& SolverBuilder::withAlgorithm(AlgorithmType type) {
    config_.algorithmType = type;
    return *this;
}

SolverBuilder& SolverBuilder::withCostModel(CostModelType type) {
    config_.costModelType = type;
    return *this;
}

SolverBuilder& SolverBuilder::withMaxEvaluations(int value) {
    config_.maxEvaluations = value;
    return *this;
}

SolverBuilder& SolverBuilder::withSequenceLength(int value) {
    config_.sequenceLength = value;
    return *this;
}

SolverBuilder& SolverBuilder::withRuntimeMeasurement(bool enabled) {
    config_.measureRuntime = enabled;
    return *this;
}

SolverBuilder& SolverBuilder::withRuntimeMeasurementNative(bool enabled) {
    config_.measureRuntimeNative = enabled;
    return *this;
}

SolverBuilder& SolverBuilder::withVerbose(bool enabled) {
    config_.verbose = enabled;
    return *this;
}

SolverBuilder& SolverBuilder::withLLVMBinPath(const std::string& path) {
    config_.llvmBinPath = path;
    return *this;
}

SolverBuilder& SolverBuilder::withCflags(const std::string& flags) {
    config_.cflags = flags;
    return *this;
}

SolverBuilder& SolverBuilder::withLogConfig(const LogConfig& logConfig) {
    logConfig_ = logConfig;
    return *this;
}

std::unique_ptr<PhaseOrderingSolver> SolverBuilder::build() {
    auto algorithm = AlgorithmFactory::create(config_.algorithmType, config_);
    auto costModel = CostModelFactory::create(config_.costModelType);
    auto evaluator = EvaluatorFactory::create(config_);

    auto solver = std::make_unique<PhaseOrderingSolver>(
        std::move(algorithm), std::move(evaluator), std::move(costModel),
        config_);

    solver->setLogConfig(logConfig_);
    if (!logConfig_.outputDir.empty() && logConfig_.outputDir != ".") {
        solver->openLogFile(logConfig_.outputDir + "/optimization.log");
    }

    return solver;
}

}  // namespace phaseordering
