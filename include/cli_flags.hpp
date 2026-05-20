/**
 * @file cli_flags.hpp
 * @brief Command-line interface configuration and parsing.
 *
 * Defines ExperimentConfig, ProgramInput, and CliConfig structures
 * for parsing CLI arguments and building experiment matrices.
 * Supports comma-separated parameter lists, sources-list files
 * with directory scanning, and project grouping.
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "data_types.hpp"

namespace phaseordering {

struct ExperimentConfig {
    AlgorithmType algorithmType;
    CostModelType costModelType;
    int maxEvaluations;
    int sequenceLength;
    std::string label;
};

struct ProgramInput {
    std::string name;
    std::vector<std::string> sourceFiles;
    std::string irContent;
    std::string cflags;
    bool isIR = false;

    std::string displayName() const {
        if (isIR) return name;
        if (sourceFiles.empty()) return name;
        std::string n = sourceFiles[0];
        auto pos = n.find_last_of("/\\");  // FIXME
        if (pos != std::string::npos) n = n.substr(pos + 1);
        if (sourceFiles.size() > 1)
            n += " (+" + std::to_string(sourceFiles.size() - 1) + ")";
        return n;
    }
};

struct CliConfig {
    std::vector<ProgramInput> programs;
    std::string cflags;

    std::vector<AlgorithmType> algorithmTypes;
    std::vector<CostModelType> costModelTypes;
    std::vector<int> maxEvaluationsList;
    std::vector<int> sequenceLengthList;

    std::string llvmBinPath;
    bool measureRuntime = false;
    bool measureRuntimeNative = false;

    int jobs = 1;
    int repeat = 1;

    std::string outputDir = ".";
    std::string csvFile;
    std::string teeLogFile;
    bool verbose = false;
    bool quiet = false;
    bool silent = false;

    LogConfig toLogConfig() const;
    SolverConfig toSolverConfig(const ExperimentConfig& ec) const;

    static CliConfig parse(int argc, char* argv[]);
    static void printUsage(const std::string& progName);

    std::vector<ExperimentConfig> buildConfigs() const;

   private:
    static void parseSourcesList(const std::string& path,
                                 const std::string& defaultCflags,
                                 std::vector<ProgramInput>& out);
};

}  // namespace phaseordering
