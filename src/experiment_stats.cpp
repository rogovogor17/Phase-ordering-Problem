#include "experiment_stats.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>

namespace phaseordering {

void ExperimentStats::addResult(const ProgramResult& result) {
    results.push_back(result);
    totalRuns++;
    if (result.success)
        successfulRuns++;
    else
        failedRuns++;
}

void ExperimentStats::compute() {
    if (results.empty()) return;

    std::vector<double> imprs;
    std::vector<double> times;
    std::vector<double> evals;
    std::vector<double> baseInstrs;
    std::vector<double> optInstrs;

    for (const auto& r : results) {
        if (!r.success) continue;
        imprs.push_back(r.improvementRatio);
        times.push_back(r.elapsedMs);
        evals.push_back(static_cast<double>(r.evaluationsUsed));
        if (r.baselineInstructions > 0)
            baseInstrs.push_back(static_cast<double>(r.baselineInstructions));
        if (r.optimizedInstructions > 0)
            optInstrs.push_back(static_cast<double>(r.optimizedInstructions));
    }

    if (imprs.empty()) return;

    avgImprovementRatio =
        std::accumulate(imprs.begin(), imprs.end(), 0.0) / imprs.size();
    maxImprovementRatio = *std::max_element(imprs.begin(), imprs.end());
    minImprovementRatio = *std::min_element(imprs.begin(), imprs.end());
    std::sort(imprs.begin(), imprs.end());
    medianImprovementRatio =
        (imprs.size() % 2 == 0)
            ? (imprs[imprs.size() / 2 - 1] + imprs[imprs.size() / 2]) / 2.0
            : imprs[imprs.size() / 2];

    avgElapsedMs =
        std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    avgEvaluationsUsed =
        std::accumulate(evals.begin(), evals.end(), 0.0) / evals.size();

    if (!baseInstrs.empty())
        avgBaselineInstructions =
            std::accumulate(baseInstrs.begin(), baseInstrs.end(), 0.0) /
            baseInstrs.size();
    if (!optInstrs.empty())
        avgOptimizedInstructions =
            std::accumulate(optInstrs.begin(), optInstrs.end(), 0.0) /
            optInstrs.size();
}

void ExperimentStats::printSummary(std::ostream& out) const {
    out << "\n========================================\n"
        << "       EXPERIMENT SUMMARY\n"
        << "========================================\n"
        << std::fixed << std::setprecision(1)
        << "  Runs:           " << totalRuns << " total, " << successfulRuns
        << " ok, " << failedRuns << " fail\n";

    if (successfulRuns == 0) {
        out << "  No successful runs.\n";
        out << "========================================\n";
        return;
    }

    out << std::fixed << std::setprecision(2) << "\n  --- Improvement ---\n"
        << "  Average:   " << avgImprovementRatio * 100.0 << "%\n"
        << "  Median:    " << medianImprovementRatio * 100.0 << "%\n"
        << "  Best:      " << maxImprovementRatio * 100.0 << "%\n"
        << "  Worst:     " << minImprovementRatio * 100.0 << "%\n";

    if (avgBaselineInstructions > 0) {
        out << "\n  --- Instructions ---\n"
            << "  Avg baseline:  " << static_cast<int>(avgBaselineInstructions)
            << "\n"
            << "  Avg optimized: " << static_cast<int>(avgOptimizedInstructions)
            << "\n"
            << "  Avg reduction: " << std::setprecision(1)
            << (avgBaselineInstructions - avgOptimizedInstructions) /
                   avgBaselineInstructions * 100.0
            << "%\n";
    }

    out << "\n  --- Performance ---\n"
        << std::setprecision(0) << "  Avg time/run:  " << avgElapsedMs
        << " ms\n"
        << "  Avg evals/run: " << avgEvaluationsUsed << "\n";

    out << "\n  Per-run results:\n"
        << "  " << std::left << std::setw(25) << "Config" << std::setw(14)
        << "File" << std::right << std::setw(5) << "Ok?" << std::setw(8)
        << "Impr%" << std::setw(9) << "ms\n"
        << "  " << std::string(61, '-') << "\n";

    for (const auto& r : results) {
        std::string name = r.sourceFile;
        auto pos = name.find_last_of("/\\");
        if (pos != std::string::npos) name = name.substr(pos + 1);
        if (name.size() > 12) name = name.substr(0, 10) + "..";

        out << "  " << std::left << std::setw(25) << r.configLabel
            << std::setw(14) << name << std::right << std::setw(5)
            << (r.success ? "OK" : "FAIL") << std::fixed << std::setprecision(1)
            << std::setw(7) << (r.success ? r.improvementRatio * 100.0 : 0.0)
            << "%" << std::setw(8) << static_cast<int>(r.elapsedMs) << "\n";
    }

    out << "========================================\n";
}

void ExperimentStats::writeRawCSV(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Cannot write CSV to: " << path << "\n";
        return;
    }
    writeRawCSV(out);
}

void ExperimentStats::writeRawCSV(std::ostream& out) const {
    out << "source_file,config_label,rep,success,algorithm,cost_model,max_eval,"
           "seq_len,"
        << "baseline_instr,opt_instr,impr_pct,"
        << "baseline_mem,opt_mem,baseline_br,opt_br,"
        << "baseline_rt_ms,opt_rt_ms,"
        << "best_score,evals,elapsed_ms,best_seq,error\n";

    for (const auto& r : results) {
        out << r.sourceFile << "," << r.configLabel << "," << r.repetition
            << "," << (r.success ? "1" : "0") << "," << r.algorithmName << ","
            << r.costModelName << "," << r.maxEvaluations << ","
            << r.sequenceLength << "," << r.baselineInstructions << ","
            << r.optimizedInstructions << "," << std::fixed
            << std::setprecision(2) << (r.improvementRatio * 100.0) << ","
            << r.baselineMemoryOps << "," << r.optimizedMemoryOps << ","
            << r.baselineBranchOps << "," << r.optimizedBranchOps << ","
            << std::setprecision(3) << r.baselineRuntimeMs << ","
            << r.optimizedRuntimeMs << "," << std::setprecision(6)
            << r.bestScore << "," << r.evaluationsUsed << ","
            << std::setprecision(1) << r.elapsedMs << ","
            << "\"" << r.bestSequence << "\","
            << "\"" << r.errorMessage << "\"\n";
    }
}

static std::string makeLabel(const ProgramResult& r) {
    if (!r.configLabel.empty()) return r.configLabel;
    return r.algorithmName + "_" + r.costModelName + "_ev" +
           std::to_string(r.maxEvaluations) + "_len" +
           std::to_string(r.sequenceLength);
}

void ExperimentStats::writeMatrixCSV(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[ERROR] Cannot write CSV to: " << path << "\n";
        return;
    }
    writeMatrixCSV(out);
}

void ExperimentStats::writeMatrixCSV(std::ostream& out) const {
    if (results.empty()) {
        out << "# No results\n";
        return;
    }

    std::set<std::string> progNames;
    std::set<std::string> configLabels;
    std::set<int> reps;

    for (const auto& r : results) {
        progNames.insert(r.sourceFile);
        configLabels.insert(makeLabel(r));
        reps.insert(r.repetition);
    }

    std::vector<std::string> progs(progNames.begin(), progNames.end());
    std::vector<std::string> configs(configLabels.begin(), configLabels.end());
    int maxRep = *std::max_element(reps.begin(), reps.end());

    // Collect baseline values per program (same for all configs, take first
    // valid)
    std::map<std::string, ProgramResult> baselinePerProg;
    for (const auto& r : results) {
        if (!r.success || r.baselineInstructions <= 0) continue;
        if (baselinePerProg.find(r.sourceFile) == baselinePerProg.end()) {
            baselinePerProg[r.sourceFile] = r;
        }
    }

    auto computeAvg =
        [](const std::vector<double>& vals) -> std::pair<double, double> {
        if (vals.empty()) return {0.0, 0.0};
        double mean =
            std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size();
        if (vals.size() == 1) return {mean, 0.0};
        double var = 0.0;
        for (double v : vals) var += (v - mean) * (v - mean);
        var /= (vals.size() - 1);
        return {mean, std::sqrt(var)};
    };

    auto writeMatrix =
        [&](const std::string& metricName,
            std::function<double(const ProgramResult&)> getter,
            std::function<double(const ProgramResult&)> baselineGetter,
            const std::string& unit, bool higherIsBetter) {
            out << "\n# " << metricName;
            if (!unit.empty()) out << " (" << unit << ")";
            if (higherIsBetter)
                out << " [higher=better]";
            else
                out << " [lower=better]";
            out << "\n";

            struct Key {
                std::string prog;
                std::string cfg;
                int rep;
            };
            auto keyHash = [](const Key& k) -> std::string {
                return k.prog + "|" + k.cfg + "|" + std::to_string(k.rep);
            };

            std::map<std::string, std::vector<double>> cells;
            for (const auto& r : results) {
                if (!r.success) continue;
                Key k{r.sourceFile, makeLabel(r), r.repetition};
                cells[keyHash(k)].push_back(getter(r));
            }

            // Write baseline rows first
            bool hasBaseline = false;
            for (const auto& prog : progs) {
                auto bit = baselinePerProg.find(prog);
                if (bit != baselinePerProg.end() &&
                    baselineGetter(bit->second) > 0) {
                    hasBaseline = true;
                    break;
                }
            }

            if (hasBaseline) {
                if (maxRep > 0) {
                    out << "program,rep";
                    for (const auto& c : configs) out << "," << c;
                    out << "\n";
                } else {
                    out << "program";
                    for (const auto& c : configs) out << "," << c;
                    out << "\n";
                }

                for (const auto& prog : progs) {
                    auto bit = baselinePerProg.find(prog);
                    double bv = (bit != baselinePerProg.end())
                                    ? baselineGetter(bit->second)
                                    : 0.0;
                    if (bv <= 0) continue;

                    if (maxRep > 0) {
                        out << prog << ",baseline";
                        for (size_t i = 0; i < configs.size(); ++i)
                            out << "," << std::fixed << std::setprecision(2)
                                << bv;
                        out << "\n";
                    } else {
                        out << prog << "_baseline";
                        for (size_t i = 0; i < configs.size(); ++i)
                            out << "," << std::fixed << std::setprecision(2)
                                << bv;
                        out << "\n";
                    }
                }
            }

            if (maxRep > 0) {
                if (!hasBaseline) {
                    out << "program,rep";
                    for (const auto& c : configs) out << "," << c;
                    out << "\n";
                }

                for (const auto& prog : progs) {
                    for (int rep = 0; rep <= maxRep; ++rep) {
                        out << prog << ",r" << rep;
                        for (const auto& cfg : configs) {
                            out << ",";
                            Key k{prog, cfg, rep};
                            auto it = cells.find(keyHash(k));
                            if (it != cells.end() && !it->second.empty()) {
                                auto [mean, stddev] = computeAvg(it->second);
                                out << std::fixed << std::setprecision(2)
                                    << mean;
                            }
                        }
                        out << "\n";
                    }
                    out << prog << ",avg";
                    for (const auto& cfg : configs) {
                        out << ",";
                        std::vector<double> allVals;
                        for (int r2 = 0; r2 <= maxRep; ++r2) {
                            Key k{prog, cfg, r2};
                            auto it = cells.find(keyHash(k));
                            if (it != cells.end()) {
                                for (double v : it->second)
                                    allVals.push_back(v);
                            }
                        }
                        if (!allVals.empty()) {
                            auto [mean, stddev] = computeAvg(allVals);
                            out << std::fixed << std::setprecision(2) << mean;
                            if (stddev > 0.005) {
                                out << " +/- " << std::setprecision(1)
                                    << stddev;
                            }
                        }
                    }
                    out << "\n";
                }
            } else {
                if (!hasBaseline) {
                    out << "program";
                    for (const auto& c : configs) out << "," << c;
                    out << "\n";
                }

                for (const auto& prog : progs) {
                    out << prog;
                    for (const auto& cfg : configs) {
                        Key k{prog, cfg, 0};
                        auto it = cells.find(keyHash(k));
                        out << ",";
                        if (it != cells.end() && !it->second.empty()) {
                            auto [mean, stddev] = computeAvg(it->second);
                            out << std::fixed << std::setprecision(2) << mean;
                        }
                    }
                    out << "\n";
                }
            }

            out << "\n";
        };

    out << std::fixed;

    auto noBaseline = [](const ProgramResult&) -> double { return 0.0; };

    writeMatrix(
        "improvement_pct",
        [](const ProgramResult& r) { return r.improvementRatio * 100.0; },
        noBaseline, "%", true);

    writeMatrix(
        "optimized_instructions",
        [](const ProgramResult& r) {
            return static_cast<double>(r.optimizedInstructions);
        },
        [](const ProgramResult& r) {
            return static_cast<double>(r.baselineInstructions);
        },
        "instr", false);

    writeMatrix(
        "elapsed_time", [](const ProgramResult& r) { return r.elapsedMs; },
        noBaseline, "ms", false);

    writeMatrix(
        "evaluations_used",
        [](const ProgramResult& r) {
            return static_cast<double>(r.evaluationsUsed);
        },
        noBaseline, "", false);

    if (std::any_of(results.begin(), results.end(), [](const ProgramResult& r) {
            return r.baselineRuntimeMs > 0;
        })) {
        writeMatrix(
            "runtime_ms",
            [](const ProgramResult& r) { return r.optimizedRuntimeMs; },
            [](const ProgramResult& r) { return r.baselineRuntimeMs; }, "ms",
            false);
    }

    if (std::any_of(results.begin(), results.end(), [](const ProgramResult& r) {
            return r.baselineMemoryOps > 0;
        })) {
        writeMatrix(
            "memory_ops",
            [](const ProgramResult& r) {
                return static_cast<double>(r.optimizedMemoryOps);
            },
            [](const ProgramResult& r) {
                return static_cast<double>(r.baselineMemoryOps);
            },
            "", false);
    }

    if (std::any_of(results.begin(), results.end(), [](const ProgramResult& r) {
            return r.baselineBranchOps > 0;
        })) {
        writeMatrix(
            "branch_ops",
            [](const ProgramResult& r) {
                return static_cast<double>(r.optimizedBranchOps);
            },
            [](const ProgramResult& r) {
                return static_cast<double>(r.baselineBranchOps);
            },
            "", false);
    }
}

}  // namespace phaseordering
