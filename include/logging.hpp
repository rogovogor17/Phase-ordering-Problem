#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "data_types.hpp"

namespace phaseordering {

/**
 * Configurable logger for the phase-ordering solver.
 *
 * Controls what information is printed to console and/or
 * written to a log file. Separates file-saving from
 * console printing so that IR can be saved without
 * flooding the terminal.
 */
class Logger {
   private:
    LogConfig config_;
    mutable std::ofstream logFile_;
    bool logToFile_ = false;

   public:
    explicit Logger(const LogConfig& config = LogConfig{});
    ~Logger();

    void setConfig(const LogConfig& config);
    void openLogFile(const std::string& path);

    /** Log an informational message. */
    void info(const std::string& message) const;
    /** Log a progress step (only shown if printProgress is set). */
    void progress(const std::string& message) const;
    /** Log a final result message. */
    void result(const std::string& message) const;
    /** Log an error message to stderr. */
    void error(const std::string& message) const;

    /** Save IR string to a file in the output directory. */
    void saveIRToFile(const std::string& ir, const std::string& filename) const;
    /** Print or save the initial (unoptimized) IR. */
    void printInitialIR(const std::string& ir) const;
    /** Print or save the final (optimized) IR. */
    void printFinalIR(const std::string& ir) const;
    /** Print a table comparing baseline and optimized metrics. */
    void printMetricsComparison(const IRMetrics& baseline,
                                const IRMetrics& optimized) const;
    /** Print a comparison of two runtimes. */
    void printRuntimeComparison(double baselineMs, double optimizedMs) const;
    /** Log a single evaluation step (only if printEachEvaluation is set). */
    void printEvaluatonResult(int evalNum, const OptSequence& seq,
                              double score) const;
    /** Print the final optimization result summary. */
    void printAlgoResult(const AlgoResult& result) const;

    bool isLoggingToFile() const;
    void writeToLogFile(const std::string& text) const;
};

}  // namespace phaseordering
