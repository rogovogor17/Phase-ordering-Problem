#include "logging.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "data_types.hpp"

namespace phaseordering {

Logger::Logger(const LogConfig& config) : config_(config) {}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::setConfig(const LogConfig& config) { config_ = config; }

void Logger::openLogFile(const std::string& path) {
    if (logFile_.is_open()) {
        logFile_.close();
    }
    logFile_.open(path);
    logToFile_ = logFile_.is_open();
}

void Logger::info(const std::string& message) const {
    std::cout << "[INFO] " << message << std::endl;
    if (logToFile_) {
        logFile_ << "[INFO] " << message << std::endl;
    }
}

void Logger::progress(const std::string& message) const {
    if (config_.printProgress) {
        std::cout << "       " << message << std::endl;
        if (logToFile_) {
            logFile_ << "[PROGRESS] " << message << std::endl;
        }
    }
}

void Logger::result(const std::string& message) const {
    std::cout << "[RESULT] " << message << std::endl;
    if (logToFile_) {
        logFile_ << "[RESULT] " << message << std::endl;
    }
}

void Logger::error(const std::string& message) const {
    std::cerr << "[ERROR] " << message << std::endl;
    if (logToFile_) {
        logFile_ << "[ERROR] " << message << std::endl;
    }
}

void Logger::saveIRToFile(const std::string& ir,
                          const std::string& filename) const {
    namespace fs = std::filesystem;
    std::string dir = config_.outputDir.empty() ? "." : config_.outputDir;

    std::error_code ec;
    fs::create_directories(dir, ec);
    std::string fullPath = dir + "/" + filename;

    std::ofstream out(fullPath);
    if (out.is_open()) {
        out << ir;
        out.close();
        std::cout << "[INFO] IR saved to " << fullPath << std::endl;
        if (logToFile_) {
            logFile_ << "[INFO] IR saved to " << fullPath << std::endl;
        }
    } else {
        std::cerr << "[ERROR] Failed to save IR to " << fullPath << std::endl;
        if (logToFile_) {
            logFile_ << "[ERROR] Failed to save IR to " << fullPath
                     << std::endl;
        }
    }
}

void Logger::printInitialIR(const std::string& ir) const {
    if (config_.saveInitialIR) {
        saveIRToFile(ir, "initial_ir.ll");
    }
    if (config_.printInitialIR) {
        std::cout << "=== Initial IR (" << ir.size()
                  << " bytes) ===" << std::endl;
        std::cout << ir << std::endl;
        if (logToFile_) {
            logFile_ << "=== Initial IR ===" << std::endl;
            logFile_ << ir << std::endl;
        }
    }
}

void Logger::printFinalIR(const std::string& ir) const {
    if (config_.saveFinalIR) {
        saveIRToFile(ir, "optimized_ir.ll");
    }
    if (config_.printFinalIR) {
        std::cout << "=== Final Optimized IR (" << ir.size()
                  << " bytes) ===" << std::endl;
        std::cout << ir << std::endl;
        if (logToFile_) {
            logFile_ << "=== Final Optimized IR ===" << std::endl;
            logFile_ << ir << std::endl;
        }
    }
}

void Logger::printMetricsComparison(const IRMetrics& baseline,
                                    const IRMetrics& optimized) const {
    if (!config_.printMetricsComparison) return;

    auto printLabel = [](const std::string& label, int width = 22) {
        return label +
               std::string(std::max(0, width - static_cast<int>(label.size())),
                           ' ');
    };

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "\n=== Metrics Comparison ===\n";
    oss << printLabel("Metric") << printLabel("Baseline")
        << printLabel("Optimized") << "Change\n";
    oss << std::string(72, '-') << "\n";

    auto printRow = [&](const std::string& name, double b, double o) {
        double change = b != 0 ? ((o - b) / b) * 100.0 : 0.0;
        std::string changeStr;
        if (change > 0)
            changeStr = "+" + std::to_string(static_cast<int>(change)) + "%";
        else if (change < 0)
            changeStr = std::to_string(static_cast<int>(change)) + "%";
        else
            changeStr = "0%";
        oss << printLabel(name)
            << printLabel(std::to_string(static_cast<int>(b)))
            << printLabel(std::to_string(static_cast<int>(o))) << changeStr
            << "\n";
    };

    printRow("Instructions", baseline.totalInstructions,
             optimized.totalInstructions);
    printRow("Basic Blocks", baseline.totalBasicBlocks,
             optimized.totalBasicBlocks);
    printRow("Functions", baseline.totalFunctions, optimized.totalFunctions);
    printRow("Avg BB Size", baseline.avgBasicBlockSize,
             optimized.avgBasicBlockSize);
    printRow("Memory Ops", baseline.memoryOps, optimized.memoryOps);
    printRow("Branch Ops", baseline.branchOps, optimized.branchOps);
    printRow("Call Ops", baseline.callOps, optimized.callOps);
    printRow("Arith Ops", baseline.arithmeticOps, optimized.arithmeticOps);

    std::cout << oss.str();
    if (logToFile_) {
        logFile_ << oss.str();
    }
}

void Logger::printRuntimeComparison(double baselineMs,
                                    double optimizedMs) const {
    if (!config_.printRuntimeComparison) return;

    double improvement = baselineMs != 0
                             ? ((baselineMs - optimizedMs) / baselineMs) * 100.0
                             : 0.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "\n=== Runtime Comparison ===\n";
    oss << "  Baseline:   " << baselineMs << " ms\n";
    oss << "  Optimized:  " << optimizedMs << " ms\n";
    if (improvement > 0) {
        oss << "  Speedup:    " << std::setprecision(1) << improvement
            << "% faster\n";
    } else {
        oss << "  Slowdown:   " << std::setprecision(1) << (-improvement)
            << "% slower\n";
    }

    std::cout << oss.str();
    if (logToFile_) {
        logFile_ << oss.str();
    }
}

void Logger::printEvaluationResult(int evalNum, const OptSequence& seq,
                                   double score) const {
    if (!config_.printEachEvaluation) return;

    std::ostringstream oss;
    oss << "Eval #" << evalNum << ": score=" << std::fixed
        << std::setprecision(4) << score << " seq=[" << seq.toString() << "]";

    std::cout << "       " << oss.str() << std::endl;
    if (logToFile_) {
        logFile_ << "[EVAL] " << oss.str() << std::endl;
    }
}

void Logger::printAlgoResult(const AlgoResult& result) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    oss << "\n========================================\n";
    oss << "       OPTIMIZATION RESULT\n";
    oss << "========================================\n";
    oss << "  Best sequence: " << result.bestSequence.toString() << "\n";
    oss << "  Best score:    " << result.bestScore << "\n";
    oss << "  Evaluations:   " << result.evaluationsUsed << "\n";

    if (result.improvementRatio != 0) {
        double pct = result.improvementRatio * 100.0;
        if (pct > 0)
            oss << "  Improvement:   +" << std::setprecision(2) << pct << "%\n";
        else
            oss << "  Improvement:   " << std::setprecision(2) << pct << "%\n";
    }

    oss << "========================================\n";

    std::cout << oss.str();
    if (logToFile_) {
        logFile_ << oss.str();
    }
}

bool Logger::isLoggingToFile() const { return logToFile_; }

void Logger::writeToLogFile(const std::string& text) const {
    if (logToFile_) {
        logFile_ << text;
        logFile_.flush();
    }
}

}  // namespace phaseordering
