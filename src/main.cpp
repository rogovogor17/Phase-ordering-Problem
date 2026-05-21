#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "cli_flags.hpp"
#include "data_types.hpp"
#include "experiment_stats.hpp"
#include "factories.hpp"
#include "llvm_facade.hpp"
#include "solver.hpp"

using namespace phaseordering;

static std::string compileProgram(const ProgramInput& prog,
                                  const std::string& llvmBinPath,
                                  const std::string& globalCflags) {
    if (prog.isIR && !prog.irContent.empty()) {
        return prog.irContent;
    }

    std::string cflags = prog.cflags.empty() ? globalCflags : prog.cflags;

    LLVMConfig cfg;
    cfg.llvmBinPath = llvmBinPath;
    cfg.extraClangFlags = cflags;
    LLVMFacade llvm(cfg);

    if (!llvm.isAvailable()) return "";
    if (prog.sourceFiles.empty()) return "";

    if (prog.sourceFiles.size() == 1) {
        return llvm.compileToIR(prog.sourceFiles[0]);
    }

    std::vector<std::string> irModules;
    for (const auto& srcFile : prog.sourceFiles) {
        std::string ir = llvm.compileToIR(srcFile);
        if (ir.empty()) {
            std::cerr << "[compileProgram] FAILED: " << srcFile << "\n  "
                      << llvm.getLastError() << "\n";
            continue;
        }
        irModules.push_back(ir);
    }

    if (irModules.size() == 1) return irModules[0];

    std::string linked = llvm.linkModules(irModules);
    if (!linked.empty()) return linked;

    // Fallback: simple text concatenation (may cause redefinition errors)
    std::ostringstream combined;
    for (size_t i = 0; i < irModules.size(); ++i) {
        std::istringstream stream(irModules[i]);
        std::string line;
        while (std::getline(stream, line)) {
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) {
                if (i == 0) combined << "\n";
                continue;
            }
            std::string trimmed = line.substr(start);
            if (i > 0) {
                if (trimmed.rfind("source_filename", 0) == 0) continue;
                if (trimmed.rfind("target datalayout", 0) == 0) continue;
                if (trimmed.rfind("target triple", 0) == 0) continue;
            }
            combined << line << "\n";
        }
    }
    return combined.str();
}

static std::string nowTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

static ProgramResult runOne(const ProgramInput& prog,
                            const ExperimentConfig& ec,
                            const SolverConfig& solverConfig, int repetition) {
    ProgramResult result;
    result.sourceFile = prog.displayName();
    result.configLabel = ec.label;
    result.repetition = repetition;
    result.algorithmName = algorithmTypeToString(ec.algorithmType);
    result.costModelName = costModelTypeToString(ec.costModelType);
    result.maxEvaluations = ec.maxEvaluations;
    result.sequenceLength = ec.sequenceLength;

    auto t0 = std::chrono::high_resolution_clock::now();

    try {
        std::string irContent;
        if (prog.isIR) {
            irContent = prog.irContent;
        } else {
            irContent = compileProgram(prog, solverConfig.llvmBinPath,
                                       solverConfig.cflags);
        }

        SolverBuilder builder;
        if (!irContent.empty()) {
            builder.withIR(irContent);
        } else if (!prog.sourceFiles.empty()) {
            builder.withSourceFile(prog.sourceFiles[0]);
        }
        builder.withAlgorithm(ec.algorithmType)
            .withCostModel(ec.costModelType)
            .withMaxEvaluations(ec.maxEvaluations)
            .withSequenceLength(ec.sequenceLength)
            .withRuntimeMeasurement(solverConfig.measureRuntime)
            .withRuntimeMeasurementNative(solverConfig.measureRuntimeNative)
            .withVerbose(solverConfig.verbose);
        if (!solverConfig.llvmBinPath.empty())
            builder.withLLVMBinPath(solverConfig.llvmBinPath);
        std::string effectiveCflags =
            prog.cflags.empty() ? solverConfig.cflags : prog.cflags;
        if (!effectiveCflags.empty()) builder.withCflags(effectiveCflags);

        LogConfig jobLog;
        jobLog.printProgress = solverConfig.verbose;
        jobLog.printMetricsComparison = false;
        jobLog.printRuntimeComparison = false;
        builder.withLogConfig(jobLog);

        auto solver = builder.build();
        AlgoResult ar;
        if (!irContent.empty()) {
            ar = solver->solveIR(irContent);
        } else {
            ar = solver->solve(prog.sourceFiles[0]);
        }

        result.elapsedMs = std::chrono::duration<double, std::milli>(
                               std::chrono::high_resolution_clock::now() - t0)
                               .count();

        if (ar.bestSequence.empty()) {
            result.success = false;
            result.errorMessage = ar.log;
            return result;
        }

        result.success = true;
        result.bestSequence = ar.bestSequence.toString();
        result.bestScore = ar.bestScore;
        result.evaluationsUsed = ar.evaluationsUsed;
        result.improvementRatio = ar.improvementRatio;

        result.optimizedInstructions = ar.bestMetrics.totalInstructions;
        result.optimizedMemoryOps = ar.bestMetrics.memoryOps;
        result.optimizedBranchOps = ar.bestMetrics.branchOps;
        result.optimizedRuntimeMs = ar.bestMetrics.executionTimeMs;

        EvaluationResult bl = solver->evaluate(OptSequence());
        if (bl.success && bl.metrics.totalInstructions > 0) {
            result.baselineInstructions = bl.metrics.totalInstructions;
            result.baselineMemoryOps = bl.metrics.memoryOps;
            result.baselineBranchOps = bl.metrics.branchOps;
            result.baselineRuntimeMs = bl.metrics.executionTimeMs;
        }

    } catch (const std::exception& e) {
        result.elapsedMs = std::chrono::duration<double, std::milli>(
                               std::chrono::high_resolution_clock::now() - t0)
                               .count();
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

int main(int argc, char* argv[]) {
    CliConfig cfg = CliConfig::parse(argc, argv);
    auto configs = cfg.buildConfigs();

    namespace fs = std::filesystem;
    if (cfg.outputDir != ".") {
        std::error_code ec;
        fs::create_directories(cfg.outputDir, ec);
    }

    std::string logPath = (cfg.outputDir == ".")
                              ? "experiment.log"
                              : cfg.outputDir + "/experiment.log";
    std::error_code ec;
    fs::path lp(logPath);
    if (!lp.parent_path().empty()) fs::create_directories(lp.parent_path(), ec);

    std::ofstream logFile(logPath, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "[ERROR] Cannot open log file: " << logPath << "\n";
    }

    std::ofstream teeFile;
    if (!cfg.teeLogFile.empty()) {
        teeFile.open(cfg.teeLogFile, std::ios::app);
        if (!teeFile.is_open()) {
            std::cerr << "[ERROR] Cannot open tee-log file: " << cfg.teeLogFile
                      << "\n";
        }
    }

    auto logLine = [&](const std::string& msg) {
        if (logFile.is_open()) {
            logFile << "[" << nowTimestamp() << "] " << msg << std::endl;
        }
        if (teeFile.is_open()) {
            teeFile << "[" << nowTimestamp() << "] " << msg << std::endl;
        }
    };

    int totalJobs =
        static_cast<int>(cfg.programs.size()) * configs.size() * cfg.repeat;

    std::ostringstream header;
    header << "=== Phase-Ordering Experiment ===\n"
           << "Programs: " << cfg.programs.size()
           << " | Configs: " << configs.size() << " | Repeats: " << cfg.repeat
           << " | Total: " << totalJobs << " | Jobs: " << cfg.jobs << "\n";
    for (size_t i = 0; i < cfg.programs.size(); ++i) {
        const auto& p = cfg.programs[i];
        header << "  [" << (i + 1) << "] " << p.displayName() << " ("
               << p.sourceFiles.size() << " file(s))";
        if (!p.cflags.empty()) header << " cflags=[" << p.cflags << "]";
        header << "\n";
    }
    for (const auto& ec : configs) header << "  " << ec.label << "\n";
    header << "=================================";
    if (!cfg.quiet && !cfg.silent) std::cout << header.str() << "\n\n";
    logLine(header.str());

    struct Job {
        ProgramInput prog;
        ExperimentConfig ec;
        SolverConfig sc;
        int rep;
    };
    std::vector<Job> jobs;
    for (const auto& ec : configs) {
        SolverConfig scJob = cfg.toSolverConfig(ec);
        for (int r = 0; r < cfg.repeat; ++r) {
            for (const auto& prog : cfg.programs) {
                jobs.push_back({prog, ec, scJob, r});
            }
        }
    }

    ExperimentStats stats;
    auto tStart = std::chrono::high_resolution_clock::now();

    int maxConcurrent = std::min(cfg.jobs, static_cast<int>(jobs.size()));
    size_t nextIdx = 0;
    std::vector<std::future<ProgramResult>> active;

    for (int i = 0; i < maxConcurrent && nextIdx < jobs.size(); ++i) {
        auto& j = jobs[nextIdx++];
        active.push_back(
            std::async(std::launch::async, runOne, j.prog, j.ec, j.sc, j.rep));
    }

    while (!active.empty()) {
        for (auto it = active.begin(); it != active.end();) {
            if (it->wait_for(std::chrono::milliseconds(50)) ==
                std::future_status::ready) {
                ProgramResult r = it->get();
                stats.addResult(r);

                std::ostringstream progress;
                progress << "[" << stats.totalRuns << "/" << totalJobs << "] "
                         << r.configLabel << " " << r.sourceFile << " r"
                         << r.repetition << " " << (r.success ? "OK" : "FAIL");
                if (r.success) {
                    int pct = static_cast<int>(r.improvementRatio * 100);
                    progress << " impr=" << pct << "%"
                             << " " << r.baselineInstructions << "->"
                             << r.optimizedInstructions;
                } else {
                    std::string err = r.errorMessage;
                    if (err.size() > 80) err = err.substr(0, 77) + "...";
                    progress << " err=" << err;
                }
                progress << " " << static_cast<int>(r.elapsedMs) << "ms";

                if (!cfg.quiet && !cfg.silent)
                    std::cout << progress.str() << "\n";
                logLine(progress.str());

                if (r.success) {
                    std::ostringstream detail;
                    detail << "  BEST " << r.sourceFile << " " << r.configLabel
                           << " r" << r.repetition << ": "
                           << r.baselineInstructions << " -> "
                           << r.optimizedInstructions << " instr ("
                           << std::fixed << std::setprecision(1)
                           << (r.improvementRatio * 100.0) << "%)"
                           << " seq=[" << r.bestSequence << "]"
                           << " evals=" << r.evaluationsUsed
                           << " time=" << static_cast<int>(r.elapsedMs) << "ms";
                    logLine(detail.str());
                } else {
                    std::string err = r.errorMessage;
                    if (err.size() > 200) err = err.substr(0, 197) + "...";
                    logLine("  FAIL " + r.sourceFile + " " + r.configLabel +
                            " r" + std::to_string(r.repetition) + ": " + err);
                }

                it = active.erase(it);
                if (nextIdx < jobs.size()) {
                    auto& nj = jobs[nextIdx++];
                    active.push_back(std::async(std::launch::async, runOne,
                                                nj.prog, nj.ec, nj.sc, nj.rep));
                }
            } else {
                ++it;
            }
        }
    }

    double totalMs = std::chrono::duration<double, std::milli>(
                         std::chrono::high_resolution_clock::now() - tStart)
                         .count();

    stats.compute();

    if (!cfg.quiet && !cfg.silent) {
        stats.printSummary(std::cout);
        std::cout << "\nTotal wall time: " << static_cast<int>(totalMs)
                  << " ms\n";
    } else if (!cfg.silent) {
        // --quiet mode: one-line summary
        if (stats.successfulRuns > 0) {
            std::cout << std::fixed << std::setprecision(1)
                      << "avg_improvement=" << stats.avgImprovementRatio * 100.0
                      << "% "
                      << "success=" << stats.successfulRuns << "/"
                      << stats.totalRuns
                      << " time=" << static_cast<int>(totalMs) << "ms\n";
        } else {
            std::cout << "all_failed " << static_cast<int>(totalMs) << "ms\n";
        }
    } else {
        // --silent mode: minimal output
        if (stats.successfulRuns > 0) {
            std::cout << "OK\n";
        } else {
            std::cerr << "FAILED\n";
        }
    }

    {
        std::ostringstream summary;
        summary << "Total wall time: " << static_cast<int>(totalMs) << " ms"
                << " | success=" << stats.successfulRuns << "/"
                << stats.totalRuns;
        logLine(summary.str());
    }

    // Log best sequences summary
    for (const auto& r : stats.results) {
        if (r.success) {
            std::ostringstream entry;
            entry << "BEST_SEQ " << r.sourceFile << " " << r.configLabel << " r"
                  << r.repetition << " improvement=" << std::fixed
                  << std::setprecision(1) << (r.improvementRatio * 100.0) << "%"
                  << " baseline_instr=" << r.baselineInstructions
                  << " optimized_instr=" << r.optimizedInstructions << " seq=["
                  << r.bestSequence << "]"
                  << " score=" << std::fixed << std::setprecision(4)
                  << r.bestScore << " evals=" << r.evaluationsUsed
                  << " time=" << static_cast<int>(r.elapsedMs) << "ms";
            logLine(entry.str());
        }
    }

    if (!cfg.csvFile.empty()) {
        fs::path csvPath(cfg.csvFile);
        if (!csvPath.parent_path().empty()) {
            std::error_code ec;
            fs::create_directories(csvPath.parent_path(), ec);
        }

        std::string matrixPath =
            csvPath.stem().string() + "_matrix" + csvPath.extension().string();
        if (csvPath.parent_path().empty()) {
            matrixPath = (cfg.outputDir == ".")
                             ? matrixPath
                             : cfg.outputDir + "/" + matrixPath;
        } else {
            matrixPath =
                (csvPath.parent_path() / fs::path(matrixPath)).string();
        }

        stats.writeRawCSV(cfg.csvFile);
        if (!cfg.silent) std::cout << "Raw data: " << cfg.csvFile << "\n";
        logLine("Raw data written to: " + cfg.csvFile);

        stats.writeMatrixCSV(matrixPath);
        if (!cfg.silent)
            std::cout << "Comparison matrix: " << matrixPath << "\n";
        logLine("Matrix written to: " + matrixPath);
    }

    if (logFile.is_open()) logFile.close();
    if (teeFile.is_open()) teeFile.close();

    return stats.successfulRuns > 0 ? 0 : 1;
}
