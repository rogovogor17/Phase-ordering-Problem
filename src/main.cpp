#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "data_types.hpp"
#include "solver.hpp"

using namespace phaseordering;

int main(int argc, char* argv[]) {
    SolverConfig config;
    LogConfig logConfig;
    std::string sourceFile;
    std::string irFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            // printUsage(argv[0]);
            return 0;
        } else if (arg == "--source" && i + 1 < argc) {
            sourceFile = argv[++i];
        } else if (arg == "--ir" && i + 1 < argc) {
            irFile = argv[++i];
        } else if (arg == "--algorithm" && i + 1 < argc) {
            std::string algo = argv[++i];
            if (algo == "random") {
                config.algorithmType = AlgorithmType::RandomSearch;
            } else if (algo == "hillclimbing" || algo == "hc") {
                config.algorithmType = AlgorithmType::HillClimbing;
            } else if (algo == "sa" || algo == "simulatedannealing") {
                config.algorithmType = AlgorithmType::SimulatedAnnealing;
            } else {
                std::cerr << "Unknown algorithm: " << algo << std::endl;
                return 1;
            }
        } else if (arg == "--costmodel" && i + 1 < argc) {
            std::string model = argv[++i];
            if (model == "weighted") {
                config.costModelType = CostModelType::Weighted;
            } else if (model == "instructions" || model == "instructioncount") {
                config.costModelType = CostModelType::InstructionCount;
            } else if (model == "runtime") {
                config.costModelType = CostModelType::Runtime;
            } else {
                std::cerr << "Unknown cost model: " << model << std::endl;
                return 1;
            }
        } else if (arg == "--max-eval" && i + 1 < argc) {
            config.maxEvaluations = std::stoi(argv[++i]);
        } else if (arg == "--seq-len" && i + 1 < argc) {
            config.sequenceLength = std::stoi(argv[++i]);
        } else if (arg == "--measure-runtime") {
            config.measureRuntime = true;
        } else if (arg == "--measure-runtime-native") {
            config.measureRuntimeNative = true;
        } else if (arg == "--llvm-path" && i + 1 < argc) {
            config.llvmBinPath = argv[++i];
        } else if (arg == "--save-initial-ir") {
            logConfig.saveInitialIR = true;
        } else if (arg == "--save-final-ir") {
            logConfig.saveFinalIR = true;
        } else if (arg == "--print-initial-ir") {
            logConfig.printInitialIR = true;
        } else if (arg == "--print-final-ir") {
            logConfig.printFinalIR = true;
        } else if (arg == "--metrics-comparison") {
            logConfig.printMetricsComparison = true;
        } else if (arg == "--no-metrics-comparison") {
            logConfig.printMetricsComparison = false;
        } else if (arg == "--runtime-comparison") {
            logConfig.printRuntimeComparison = true;
        } else if (arg == "--no-runtime-comparison") {
            logConfig.printRuntimeComparison = false;
        } else if (arg == "--print-progress") {
            logConfig.printProgress = true;
        } else if (arg == "--print-each-eval") {
            logConfig.printEachEvaluation = true;
        } else if (arg == "--output-dir" && i + 1 < argc) {
            logConfig.outputDir = argv[++i];
        } else if (arg == "--verbose") {
            config.verbose = true;
            logConfig.printProgress = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            // printUsage(argv[0]);
            return 1;
        }
    }

    if (sourceFile.empty() && irFile.empty()) {
        std::cerr << "Error: either --source or --ir must be specified\n\n";
        // printUsage(argv[0]);
        return 1;
    }

    try {
        SolverBuilder builder;

        if (!sourceFile.empty()) {
            builder.withSourceFile(sourceFile);
        }
        if (!irFile.empty()) {
            std::ifstream irStream(irFile);
            if (!irStream.is_open()) {
                std::cerr << "Error: cannot open IR file: " << irFile
                          << std::endl;
                return 1;
            }
            std::string irContent((std::istreambuf_iterator<char>(irStream)),
                                  std::istreambuf_iterator<char>());
            builder.withIR(irContent);
        }

        builder.withAlgorithm(config.algorithmType)
            .withCostModel(config.costModelType)
            .withMaxEvaluations(config.maxEvaluations)
            .withSequenceLength(config.sequenceLength)
            .withRuntimeMeasurement(config.measureRuntime)
            .withRuntimeMeasurementNative(config.measureRuntimeNative)
            .withVerbose(config.verbose);

        if (!config.llvmBinPath.empty()) {
            builder.withLLVMBinPath(config.llvmBinPath);
        }

        builder.withLogConfig(logConfig);

        auto solver = builder.build();

        AlgoResult result;
        if (!sourceFile.empty()) {
            result = solver->solve(sourceFile);
        } else {
            std::ifstream irStream(irFile);
            std::string irContent((std::istreambuf_iterator<char>(irStream)),
                                  std::istreambuf_iterator<char>());
            result = solver->solveIR(irContent);
        }

        return result.evaluationsUsed > 0 ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
