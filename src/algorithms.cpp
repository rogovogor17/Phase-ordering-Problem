#include "algorithms.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>

#include "data_types.hpp"
#include "sequence_ops.hpp"

namespace phaseordering {

/*
 * RandomSearch: try random sequences, keep the best one
 */
RandomSearch::RandomSearch(int sequenceLength, int maxSequenceLength)
    : sequenceLength_(sequenceLength),
      maxSequenceLength_(maxSequenceLength),
      registry_(std::make_unique<PassRegistry>()),
      generator_(std::make_unique<SequenceGenerator>(registry_.get())) {}

AlgoResult RandomSearch::run(const AlgoContext& ctx) {
    std::random_device rd;
    std::mt19937 rng(rd());

    // Validate context before doing anything.
    if (!ctx.evaluator || !ctx.costModel) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log = "Invalid context: evaluator or cost model is null\n";
        return fail;
    }
    if (ctx.maxEvaluations <= 0) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log = "Invalid context: maxEvaluations must be > 0\n";
        return fail;
    }

    static const int MAX_REASONABLE_LEN = kMaxSequenceLength;
    int effectiveLen = ctx.maxSequenceLength > 0
                           ? std::min({sequenceLength_, ctx.maxSequenceLength,
                                       MAX_REASONABLE_LEN})
                           : std::min(sequenceLength_, MAX_REASONABLE_LEN);
    int len = std::max(1, effectiveLen);

    // bestScore must start at sentinel so any real score wins.
    // The empty-sequence "win" bug happened partly because bestScore was
    // initialized to 1e9 and empty sequence score was also 1e9 — when we
    // accidentally fed a raw IR that scored 1.0 it would win over everything.
    OptSequence bestSequence;
    double bestScore = std::numeric_limits<double>::max();
    IRMetrics bestMetrics;
    std::ostringstream log;

    // Try known-good starting points (O2, O1, short random)
    OptSequence candidates[] = {
        generator_->generateO2(), generator_->generateO1(),
        generator_->generateRandom(std::min(3, len), rng)};

    for (auto& candidate : candidates) {
        if (candidate.empty()) continue;

        EvaluationResult eval = ctx.evaluator->evaluate(candidate);
        if (!eval.success) continue;
        if (eval.metrics.totalInstructions <= 0) continue;

        double score = ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
        if (score >= 1e8) continue;

        if (bestSequence.empty() || ctx.costModel->isBetter(score, bestScore)) {
            bestSequence = candidate;
            bestScore = score;
            bestMetrics = eval.metrics;
        }
    }

    int attempts = 0;
    int successes = 0;

    if (!bestSequence.empty()) {
        successes = 1;
        log << "[1] Start: score=" << std::fixed << std::setprecision(4)
            << bestScore << " len=" << bestSequence.size() << " seq=["
            << bestSequence.toString() << "]\n";
    }

    // Phase 1: random exploration (50% of budget)
    int randomBudget = ctx.maxEvaluations / 2;
    for (int i = 0; i < randomBudget; ++i) {
        attempts++;
        int tryLen = 3 + (rng() % std::max(1, len - 3 + 1));
        OptSequence candidate = generator_->generateRandom(tryLen, rng);

        if (candidate.empty()) continue;

        EvaluationResult evalResult = ctx.evaluator->evaluate(candidate);
        if (!evalResult.success) continue;
        if (evalResult.metrics.totalInstructions <= 0) continue;

        double score =
            ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
        if (score >= 1e8) continue;

        successes++;
        if (bestSequence.empty() || ctx.costModel->isBetter(score, bestScore)) {
            bestScore = score;
            bestSequence = candidate;
            bestMetrics = evalResult.metrics;
            log << "[" << successes << "] score=" << std::fixed
                << std::setprecision(4) << score << " len=" << candidate.size()
                << " seq=[" << candidate.toString() << "]\n";
        }
    }

    // Phase 2: exploit best by mutation (50% of budget)
    if (!bestSequence.empty()) {
        SequenceMutator mutator(registry_.get(), maxSequenceLength_);
        OptSequence current = bestSequence;
        double currentScore = bestScore;

        for (int i = 0; i < randomBudget && attempts < ctx.maxEvaluations;
             ++i) {
            attempts++;
            OptSequence neighbor = mutator.mutate(current, rng);

            if (neighbor.empty()) continue;

            EvaluationResult evalResult = ctx.evaluator->evaluate(neighbor);
            if (!evalResult.success) continue;
            if (evalResult.metrics.totalInstructions <= 0) continue;

            double score =
                ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
            if (score >= 1e8) continue;

            successes++;
            if (ctx.costModel->isBetter(score, currentScore)) {
                current = neighbor;
                currentScore = score;
                if (ctx.costModel->isBetter(score, bestScore)) {
                    bestScore = score;
                    bestSequence = neighbor;
                    bestMetrics = evalResult.metrics;
                    log << "[" << successes << "] score=" << std::fixed
                        << std::setprecision(4) << score
                        << " len=" << neighbor.size() << " seq=["
                        << neighbor.toString() << "]\n";
                }
            }
        }
    }

    // Distinguish "all evaluations failed" from "no improvement found".
    // Previously an empty bestSequence was returned silently.
    if (bestSequence.empty()) {
        log << "ERROR: No successful evaluations out of " << attempts
            << " attempts — all pass sequences failed or produced invalid IR.\n"
            << "Check that the input IR is valid and LLVM opt is working "
               "correctly.\n";
    } else {
        log << "Best: score=" << std::fixed << std::setprecision(4) << bestScore
            << " (" << successes << "/" << attempts << " ok)\n";
    }

    AlgoResult result;
    result.bestSequence = bestSequence;
    result.bestMetrics = bestMetrics;
    result.bestScore = bestSequence.empty() ? 1e9 : bestScore;
    result.evaluationsUsed = successes;
    result.improvementRatio = 0.0;
    result.log = log.str();
    return result;
}

/*
 * HillClimbing: start from a good sequence, mutate to improve
 */
HillClimbing::HillClimbing(int maxNoImprove, int maxSequenceLength)
    : maxNoImprove_(maxNoImprove),
      maxSequenceLength_(maxSequenceLength),
      registry_(std::make_unique<PassRegistry>()),
      generator_(std::make_unique<SequenceGenerator>(registry_.get())),
      mutator_(std::make_unique<SequenceMutator>(registry_.get(),
                                                 maxSequenceLength)) {}

AlgoResult HillClimbing::run(const AlgoContext& ctx) {
    std::random_device rd;
    std::mt19937 rng(rd());

    if (!ctx.evaluator || !ctx.costModel) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log = "Invalid context: evaluator or cost model is null\n";
        return fail;
    }
    if (ctx.maxEvaluations <= 0) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log = "Invalid context: maxEvaluations must be > 0\n";
        return fail;
    }

    int effectiveMaxNoImprove =
        std::max(30, std::min(maxNoImprove_, ctx.maxEvaluations / 4));
    static const int MAX_RESTARTS = 3;

    OptSequence bestSequence;
    double bestScore = std::numeric_limits<double>::max();
    IRMetrics bestMetrics;
    int totalAttempts = 0;
    int successes = 0;
    std::ostringstream log;
    int restarts = 0;

    while (totalAttempts < ctx.maxEvaluations && restarts <= MAX_RESTARTS) {
        OptSequence current;
        double currentScore = std::numeric_limits<double>::max();
        IRMetrics currentMetrics;

        if (restarts == 0) {
            // Try known good starting points
            OptSequence candidates[] = {generator_->generateO2(),
                                        generator_->generateO1(),
                                        generator_->generateRandom(3, rng)};
            for (auto& candidate : candidates) {
                if (candidate.empty()) continue;
                EvaluationResult eval = ctx.evaluator->evaluate(candidate);
                if (!eval.success) continue;
                if (eval.metrics.totalInstructions <= 0) continue;
                double score =
                    ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
                if (score >= 1e8) continue;
                if (current.empty() ||
                    ctx.costModel->isBetter(score, currentScore)) {
                    current = candidate;
                    currentScore = score;
                    currentMetrics = eval.metrics;
                }
            }
            if (current.empty()) {
                // Return a meaningful failure result, not a bare default
                AlgoResult failResult;
                failResult.bestScore = 1e9;
                failResult.evaluationsUsed = 0;
                failResult.log =
                    "All starting sequences failed to evaluate.\n"
                    "The input IR may be invalid or LLVM opt may not support "
                    "the requested passes.\n";
                return failResult;
            }
            bestSequence = current;
            bestScore = currentScore;
            bestMetrics = currentMetrics;
            log << "Start: score=" << std::fixed << std::setprecision(4)
                << currentScore << " len=" << current.size() << " seq=["
                << current.toString() << "]\n";
        } else {
            // Restart: perturb the best sequence with random mutations
            current = bestSequence;
            for (int shake = 0; shake < 5; ++shake) {
                OptSequence shaken = mutator_->mutate(current, rng);
                if (!shaken.empty()) current = shaken;
            }
            EvaluationResult eval = ctx.evaluator->evaluate(current);
            if (eval.success && eval.metrics.totalInstructions > 0) {
                double score =
                    ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
                if (score < 1e8) {
                    currentScore = score;
                    currentMetrics = eval.metrics;
                }
            }
            if (currentScore >= 1e8) {
                // Perturbed restart failed; try a fully random sequence
                OptSequence randomSeq = generator_->generateRandom(
                    std::max(3, std::min(10, ctx.maxSequenceLength)), rng);
                if (!randomSeq.empty()) {
                    EvaluationResult eval2 = ctx.evaluator->evaluate(randomSeq);
                    if (eval2.success && eval2.metrics.totalInstructions > 0) {
                        double score2 = ctx.costModel->score(
                            eval2.metrics, ctx.baselineMetrics);
                        if (score2 < 1e8) {
                            current = randomSeq;
                            currentScore = score2;
                            currentMetrics = eval2.metrics;
                        }
                    }
                }
            }
            if (currentScore >= 1e8) {
                restarts++;
                continue;
            }
            if (ctx.costModel->isBetter(currentScore, bestScore)) {
                bestScore = currentScore;
                bestSequence = current;
                bestMetrics = currentMetrics;
            }
            log << "[Restart " << restarts << "] score=" << std::fixed
                << std::setprecision(4) << currentScore
                << " len=" << current.size() << "\n";
        }

        int noImproveCount = 0;
        while (noImproveCount < effectiveMaxNoImprove &&
               totalAttempts < ctx.maxEvaluations) {
            OptSequence neighbor = mutator_->mutate(current, rng);
            totalAttempts++;

            if (neighbor.empty()) {
                noImproveCount++;
                continue;
            }

            EvaluationResult evalResult = ctx.evaluator->evaluate(neighbor);
            if (!evalResult.success) {
                noImproveCount++;
                continue;
            }
            if (evalResult.metrics.totalInstructions <= 0) {
                noImproveCount++;
                continue;
            }

            double neighborScore =
                ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
            if (neighborScore >= 1e8) {
                noImproveCount++;
                continue;
            }

            successes++;

            if (ctx.costModel->isBetter(neighborScore, currentScore)) {
                current = neighbor;
                currentScore = neighborScore;
                currentMetrics = evalResult.metrics;
                noImproveCount = 0;

                if (ctx.costModel->isBetter(neighborScore, bestScore)) {
                    bestSequence = neighbor;
                    bestScore = neighborScore;
                    bestMetrics = evalResult.metrics;
                    log << "[" << successes
                        << "] New best: score=" << std::fixed
                        << std::setprecision(4) << bestScore
                        << " len=" << neighbor.size() << " seq=["
                        << neighbor.toString() << "]\n";
                }
            } else {
                noImproveCount++;
            }
        }
        restarts++;
    }

    if (bestSequence.empty()) {
        log << "ERROR: Hill climbing failed to find any valid non-empty "
               "sequence.\n";
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.evaluationsUsed = 0;
        fail.log = log.str();
        return fail;
    }

    log << "Best: score=" << std::fixed << std::setprecision(4) << bestScore
        << " (" << successes << "/" << totalAttempts << " ok, "
        << (restarts - 1) << " restarts)\n";

    AlgoResult result;
    result.bestSequence = bestSequence;
    result.bestMetrics = bestMetrics;
    result.bestScore = bestScore;
    result.evaluationsUsed = successes;
    result.improvementRatio = 0.0;
    result.log = log.str();
    return result;
}

/*
 * SimulatedAnnealing
 */
SimulatedAnnealing::SimulatedAnnealing(double startTemperature,
                                       double coolingRate,
                                       int maxSequenceLength)
    : startTemperature_(startTemperature),
      coolingRate_(coolingRate),
      maxSequenceLength_(maxSequenceLength),
      registry_(std::make_unique<PassRegistry>()),
      generator_(std::make_unique<SequenceGenerator>(registry_.get())),
      mutator_(std::make_unique<SequenceMutator>(registry_.get(),
                                                 maxSequenceLength)) {}

AlgoResult SimulatedAnnealing::run(const AlgoContext& ctx) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> probDist(0.0, 1.0);

    if (!ctx.evaluator || !ctx.costModel) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log = "Invalid context: evaluator or cost model is null\n";
        return fail;
    }
    if (ctx.maxEvaluations <= 0) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log = "Invalid context: maxEvaluations must be > 0\n";
        return fail;
    }

    if (startTemperature_ <= 0.0 || coolingRate_ <= 0.0 ||
        coolingRate_ >= 1.0) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.log =
            "Invalid SA parameters: startTemperature must be > 0, "
            "coolingRate must be in (0, 1)\n";
        return fail;
    }

    OptSequence current;
    double currentScore = std::numeric_limits<double>::max();
    IRMetrics currentMetrics;

    OptSequence candidates[] = {
        generator_->generateO2(), generator_->generateO1(),
        generator_->generateRandom(
            std::min(3, ctx.maxSequenceLength > 0 ? ctx.maxSequenceLength : 3),
            rng)};

    for (auto& candidate : candidates) {
        if (candidate.empty()) continue;
        EvaluationResult eval = ctx.evaluator->evaluate(candidate);
        if (!eval.success) continue;
        if (eval.metrics.totalInstructions <= 0) continue;
        double score = ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
        if (score >= 1e8) continue;
        if (current.empty() || ctx.costModel->isBetter(score, currentScore)) {
            current = candidate;
            currentScore = score;
            currentMetrics = eval.metrics;
        }
    }

    if (current.empty()) {
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.evaluationsUsed = 0;
        fail.log =
            "All starting sequences failed to evaluate.\n"
            "The input IR may be invalid or LLVM opt may not support "
            "the requested passes.\n";
        return fail;
    }

    OptSequence bestSequence = current;
    double bestScore = currentScore;
    IRMetrics bestMetrics = currentMetrics;

    double temperature = startTemperature_;
    int totalAttempts = 0;
    int successes = 0;
    std::ostringstream log;

    log << "Start: score=" << std::fixed << std::setprecision(4) << currentScore
        << " len=" << current.size() << " seq=[" << current.toString() << "]\n";

    while (totalAttempts < ctx.maxEvaluations && temperature > 0.01) {
        for (int i = 0; i < 5 && totalAttempts < ctx.maxEvaluations; ++i) {
            OptSequence neighbor = mutator_->mutate(current, rng);
            totalAttempts++;

            if (neighbor.empty()) continue;

            EvaluationResult evalResult = ctx.evaluator->evaluate(neighbor);
            if (!evalResult.success) continue;
            if (evalResult.metrics.totalInstructions <= 0) continue;

            double neighborScore =
                ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
            if (neighborScore >= 1e8) continue;

            successes++;

            double delta = neighborScore - currentScore;
            // Only accept worse solutions if current temperature is
            // meaningful. When delta < 0, it's strictly better — always accept.
            bool accept =
                (delta < 0) || (temperature > 1e-10 &&
                                probDist(rng) < std::exp(-delta / temperature));

            if (accept) {
                current = neighbor;
                currentScore = neighborScore;
                currentMetrics = evalResult.metrics;

                if (ctx.costModel->isBetter(neighborScore, bestScore)) {
                    bestSequence = neighbor;
                    bestScore = neighborScore;
                    bestMetrics = evalResult.metrics;
                    log << "[" << successes
                        << "] New best: score=" << std::fixed
                        << std::setprecision(4) << bestScore
                        << " len=" << neighbor.size() << " seq=["
                        << neighbor.toString() << "]\n";
                }
            }
        }
        temperature *= coolingRate_;
    }

    if (bestSequence.empty()) {
        log << "ERROR: Simulated annealing failed to find any valid non-empty "
               "sequence.\n";
        AlgoResult fail;
        fail.bestScore = 1e9;
        fail.evaluationsUsed = 0;
        fail.log = log.str();
        return fail;
    }

    log << "Best: score=" << std::fixed << std::setprecision(4) << bestScore
        << " (" << successes << "/" << totalAttempts << " ok)\n";

    AlgoResult result;
    result.bestSequence = bestSequence;
    result.bestMetrics = bestMetrics;
    result.bestScore = bestScore;
    result.evaluationsUsed = successes;
    result.improvementRatio = 0.0;
    result.log = log.str();
    return result;
}

}  // namespace phaseordering
