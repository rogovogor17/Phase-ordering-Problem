#include "cli_flags.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace phaseordering {

LogConfig CliConfig::toLogConfig() const {
    LogConfig lc;
    lc.outputDir = outputDir;
    lc.printProgress = verbose && !quiet;
    lc.printEachEvaluation = verbose && !quiet;
    lc.printMetricsComparison = !quiet;
    lc.printRuntimeComparison = !quiet;
    lc.saveInitialIR = verbose;
    lc.saveFinalIR = verbose;
    lc.printInitialIR = false;
    lc.printFinalIR = false;
    return lc;
}

SolverConfig CliConfig::toSolverConfig(const ExperimentConfig& ec) const {
    SolverConfig sc;
    sc.algorithmType = ec.algorithmType;
    sc.costModelType = ec.costModelType;
    sc.maxEvaluations = ec.maxEvaluations;
    sc.sequenceLength = ec.sequenceLength;
    sc.measureRuntime = measureRuntime;
    sc.measureRuntimeNative = measureRuntimeNative;
    sc.llvmBinPath = llvmBinPath;
    sc.cflags = cflags;
    sc.verbose = verbose;
    return sc;
}

std::vector<ExperimentConfig> CliConfig::buildConfigs() const {
    std::vector<ExperimentConfig> configs;
    for (auto algo : algorithmTypes) {
        for (auto cm : costModelTypes) {
            for (int maxEval : maxEvaluationsList) {
                for (int seqLen : sequenceLengthList) {
                    ExperimentConfig ec;
                    ec.algorithmType = algo;
                    ec.costModelType = cm;
                    ec.maxEvaluations = maxEval;
                    ec.sequenceLength = seqLen;
                    ec.label = algorithmTypeToString(algo) + "_" +
                               costModelTypeToString(cm) + "_" + "ev" +
                               std::to_string(maxEval) + "_" + "len" +
                               std::to_string(seqLen);
                    configs.push_back(ec);
                }
            }
        }
    }
    return configs;
}

void CliConfig::parseSourcesList(const std::string& path,
                                 const std::string& defaultCflags,
                                 std::vector<ProgramInput>& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: cannot open sources list: " << path << "\n";
        std::exit(1);
    }

    std::string line;
    std::string currentProject;
    std::vector<std::string> currentFiles;
    std::string currentCflags = defaultCflags;

    auto flushProject = [&]() {
        if (!currentFiles.empty()) {
            ProgramInput pi;
            pi.name = currentProject.empty() ? currentFiles[0] : currentProject;
            pi.sourceFiles = currentFiles;
            pi.cflags = currentCflags;
            out.push_back(pi);
            currentFiles.clear();
            currentCflags = defaultCflags;
        }
    };

    auto isSourceFile = [](const std::string& p) -> bool {
        std::string ext = p.substr(p.find_last_of(".\\/") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" ||
               ext == "c++";
    };

    auto scanDirectory =
        [&isSourceFile](
            const std::string& dirPath) -> std::vector<std::string> {
        std::vector<std::string> files;
        namespace fs = std::filesystem;
        std::error_code ec;
        for (const auto& entry :
             fs::recursive_directory_iterator(dirPath, ec)) {
            if (entry.is_regular_file() &&
                isSourceFile(entry.path().string())) {
                files.push_back(entry.path().string());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    };

    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' ||
                                 line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        while (!line.empty() && (line[0] == ' ' || line[0] == '\t'))
            line.erase(line.begin());

        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '@') {
            flushProject();
            line = line.substr(1);
            if (!line.empty() && line.back() == ':') line.pop_back();

            currentProject = line;
            currentCflags = defaultCflags;

            size_t flagPos = line.find(" -");
            if (flagPos != std::string::npos) {
                currentProject = line.substr(0, flagPos);
                currentCflags = line.substr(flagPos + 1);
                if (!defaultCflags.empty())
                    currentCflags += " " + defaultCflags;
            }
            continue;
        }

        if (line.find("CFLAGS=") == 0 || line.find("cflags=") == 0) {
            size_t eq = line.find('=');
            currentCflags = line.substr(eq + 1);
            if (!defaultCflags.empty()) currentCflags += " " + defaultCflags;
            continue;
        }

        // Check if path is a directory
        namespace fs = std::filesystem;
        std::error_code ec;
        if (fs::is_directory(line, ec) && !ec) {
            auto dirFiles = scanDirectory(line);
            if (dirFiles.empty()) {
                std::cerr << "Warning: directory '" << line
                          << "' contains no source files\n";
            }
            for (const auto& df : dirFiles) {
                currentFiles.push_back(df);
            }
        } else {
            currentFiles.push_back(line);
        }
    }
    flushProject();
}

CliConfig CliConfig::parse(int argc, char* argv[]) {
    CliConfig config;
    std::string progName = argv[0];

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(progName);
            std::exit(0);
        } else if (arg == "--source" && i + 1 < argc) {
            ProgramInput pi;
            std::string sourcePath = argv[++i];
            namespace fs = std::filesystem;
            std::error_code ec;
            if (fs::is_directory(sourcePath, ec) && !ec) {
                pi.name = sourcePath;
                for (const auto& entry :
                     fs::recursive_directory_iterator(sourcePath, ec)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(),
                                       ::tolower);
                        if (ext == ".c" || ext == ".cpp" || ext == ".cc" ||
                            ext == ".cxx" || ext == ".c++") {
                            pi.sourceFiles.push_back(entry.path().string());
                        }
                    }
                }
                std::sort(pi.sourceFiles.begin(), pi.sourceFiles.end());
                if (pi.sourceFiles.empty()) {
                    std::cerr << "Warning: directory '" << sourcePath
                              << "' contains no source files\n";
                }
            } else {
                pi.sourceFiles.push_back(sourcePath);
                pi.name = sourcePath;
            }
            pi.cflags = config.cflags;
            config.programs.push_back(pi);
        } else if (arg == "--sources-list" && i + 1 < argc) {
            parseSourcesList(argv[++i], config.cflags, config.programs);
        } else if (arg == "--ir" && i + 1 < argc) {
            ProgramInput pi;
            pi.name = argv[i + 1];
            pi.isIR = true;
            std::ifstream f(argv[++i]);
            if (!f.is_open()) {
                std::cerr << "Error: cannot open IR file: " << pi.name << "\n";
                std::exit(1);
            }
            pi.irContent.assign((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            if (pi.irContent.empty()) {
                std::cerr << "Error: IR file is empty\n";
                std::exit(1);
            }
            if (pi.irContent.find("define ") == std::string::npos) {
                std::cerr << "Error: IR has no functions\n";
                std::exit(1);
            }
            config.programs.push_back(pi);
        } else if (arg == "--algorithm" && i + 1 < argc) {
            std::string val = argv[++i];
            std::istringstream ss(val);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                if (tok == "random")
                    config.algorithmTypes.push_back(
                        AlgorithmType::RandomSearch);
                else if (tok == "hillclimbing" || tok == "hc")
                    config.algorithmTypes.push_back(
                        AlgorithmType::HillClimbing);
                else if (tok == "sa" || tok == "simulatedannealing")
                    config.algorithmTypes.push_back(
                        AlgorithmType::SimulatedAnnealing);
                else {
                    std::cerr << "Unknown algorithm: " << tok << "\n";
                    std::exit(1);
                }
            }
        } else if (arg == "--costmodel" && i + 1 < argc) {
            std::string val = argv[++i];
            std::istringstream ss(val);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                if (tok == "weighted")
                    config.costModelTypes.push_back(CostModelType::Weighted);
                else if (tok == "instructions" || tok == "instructioncount")
                    config.costModelTypes.push_back(
                        CostModelType::InstructionCount);
                else if (tok == "runtime")
                    config.costModelTypes.push_back(CostModelType::Runtime);
                else {
                    std::cerr << "Unknown cost model: " << tok << "\n";
                    std::exit(1);
                }
            }
        } else if (arg == "--max-eval" && i + 1 < argc) {
            std::istringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                try {
                    int v = std::stoi(tok);
                    if (v <= 0) {
                        std::cerr << "Error: --max-eval must be positive\n";
                        std::exit(1);
                    }
                    config.maxEvaluationsList.push_back(v);
                } catch (...) {
                    std::cerr << "Error: invalid --max-eval value: " << tok
                              << "\n";
                    std::exit(1);
                }
            }
        } else if (arg == "--seq-len" && i + 1 < argc) {
            std::istringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                try {
                    int v = std::stoi(tok);
                    if (v <= 0) {
                        std::cerr << "Error: --seq-len must be positive\n";
                        std::exit(1);
                    }
                    config.sequenceLengthList.push_back(v);
                } catch (...) {
                    std::cerr << "Error: invalid --seq-len value: " << tok
                              << "\n";
                    std::exit(1);
                }
            }
        } else if (arg == "--measure-runtime") {
            config.measureRuntime = true;
        } else if (arg == "--measure-runtime-native") {
            config.measureRuntimeNative = true;
        } else if (arg == "--llvm-path" && i + 1 < argc) {
            config.llvmBinPath = argv[++i];
        } else if (arg == "--cflags" && i + 1 < argc) {
            config.cflags = argv[++i];
        } else if (arg == "--jobs" && i + 1 < argc) {
            try {
                config.jobs = std::stoi(argv[++i]);
                if (config.jobs <= 0) {
                    std::cerr << "Error: --jobs must be positive\n";
                    std::exit(1);
                }
            } catch (...) {
                std::cerr << "Error: invalid --jobs value\n";
                std::exit(1);
            }
        } else if (arg == "--repeat" && i + 1 < argc) {
            try {
                config.repeat = std::stoi(argv[++i]);
                if (config.repeat <= 0) {
                    std::cerr << "Error: --repeat must be positive\n";
                    std::exit(1);
                }
            } catch (...) {
                std::cerr << "Error: invalid --repeat value\n";
                std::exit(1);
            }
        } else if (arg == "--output-dir" && i + 1 < argc) {
            config.outputDir = argv[++i];
        } else if (arg == "--csv" && i + 1 < argc) {
            config.csvFile = argv[++i];
        } else if (arg == "--tee-log" && i + 1 < argc) {
            config.teeLogFile = argv[++i];
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--quiet") {
            config.quiet = true;
        } else if (arg == "--silent") {
            config.silent = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(progName);
            std::exit(1);
        }
    }

    if (config.algorithmTypes.empty())
        config.algorithmTypes.push_back(AlgorithmType::SimulatedAnnealing);
    if (config.costModelTypes.empty())
        config.costModelTypes.push_back(CostModelType::Weighted);
    if (config.maxEvaluationsList.empty())
        config.maxEvaluationsList.push_back(300);
    if (config.sequenceLengthList.empty())
        config.sequenceLengthList.push_back(20);

    if (config.programs.empty()) {
        std::cerr << "Error: no input files specified. Use --source, "
                     "--sources-list, or --ir\n\n";
        printUsage(progName);
        std::exit(1);
    }

    return config;
}

void CliConfig::printUsage(const std::string& progName) {
    std::cout
        << "Usage: " << progName << " [options]\n\n"
        << "Phase-Ordering Experiment Runner\n\n"
        << "Input:\n"
        << "  --source <path>              Source file or directory (scans "
           ".c/.cpp recursively)\n"
        << "  --sources-list <file>        File with programs list (see format "
           "below)\n"
        << "  --ir <file>                  Input LLVM IR file\n"
        << "  --cflags <flags>             Extra compiler flags (e.g. -I "
           "include -std=c++17)\n\n"
        << "Algorithm (comma-separated for multiple):\n"
        << "  --algorithm <list>           random, hillclimbing, sa\n"
        << "  --costmodel <list>            weighted, instructions, runtime\n"
        << "  --max-eval <list>             Comma-separated, e.g. 50,100,200\n"
        << "  --seq-len <list>              Comma-separated, e.g. 3,5,8\n\n"
        << "Execution:\n"
        << "  --jobs <N>                   Parallel jobs (default: 1)\n"
        << "  --repeat <N>                 Repeat each config N times "
           "(default: 1)\n\n"
        << "LLVM:\n"
        << "  --llvm-path <path>           Path to LLVM bin directory\n"
        << "  --measure-runtime            Enable runtime measurement\n"
        << "  --measure-runtime-native     Compile & run native executables\n\n"
        << "Output:\n"
        << "  --output-dir <dir>            Output directory (default: .)\n"
        << "  --csv <file>                  Write results as CSV\n"
        << "  --tee-log <file>              Copy log to additional file (e.g. "
           "for monitoring)\n"
        << "  --verbose                    Verbose output\n"
        << "  --quiet                      Minimal output (summary only)\n"
        << "  --silent                     No console output at all\n\n"
        << "Sources list file format:\n"
        << "  # Comment\n"
        << "  single_file.cpp\n"
        << "  src/                        # directory: all .c/.cpp files "
           "recursively\n"
        << "  @project_name -I include -std=c++17:\n"
        << "      src/main.cpp\n"
        << "      src/utils.cpp\n"
        << "  @mylib src/                 # directory as project source\n"
        << "  CFLAGS=-O2 -I common\n"
        << "  another_file.c\n\n"
        << "Examples:\n"
        << "  " << progName << " --source test.cpp\n"
        << "  " << progName
        << " --source a.cpp --source b.cpp --algorithm hc,sa --repeat 5\n"
        << "  " << progName
        << " --sources-list benchmarks.txt --max-eval 50,100,200 --csv "
           "out.csv\n"
        << "  " << progName
        << " --source src/main.cpp --cflags \"-I include -std=c++17\"\n";
}

}  // namespace phaseordering
