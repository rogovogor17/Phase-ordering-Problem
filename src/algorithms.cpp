#include "algorithms.hpp"
#include "sequence_ops.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace phaseordering {

// 
// RandomSearch: try random sequences, keep the best one
// 

RandomSearch::RandomSearch(int sequenceLength, int maxSequenceLength)
    : sequenceLength_(sequenceLength)
    , maxSequenceLength_(maxSequenceLength)
    , registry_(std::make_unique<PassRegistry>())
    , generator_(std::make_unique<SequenceGenerator>(registry_.get())) {}

AlgoResult RandomSearch::run(const AlgoContext& ctx) {
    std::random_device rd;
    std::mt19937 rng(rd());

    static const int MAX_REASONABLE_LEN = 20;
    int effectiveLen = ctx.maxSequenceLength > 0
        ? std::min({sequenceLength_, ctx.maxSequenceLength, MAX_REASONABLE_LEN})
        : std::min(sequenceLength_, MAX_REASONABLE_LEN);
    int len = std::max(1, effectiveLen);

    OptSequence bestSequence;
    double bestScore = 1e9;
    IRMetrics bestMetrics;
    std::ostringstream log;

    OptSequence candidates[] = {
        generator_->generateO2(),
        generator_->generateO1(),
        generator_->generateRandom(std::min(3, len), rng)
    };

    for (auto& candidate : candidates) {
        EvaluationResult eval = ctx.evaluator->evaluate(candidate);
        if (eval.success && eval.metrics.totalInstructions > 0) {
            double score = ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
            if (score < 1e8 && ctx.costModel->isBetter(score, bestScore)) {
                bestSequence = candidate;
                bestScore = score;
                bestMetrics = eval.metrics;
            }
        }
    }

    int attempts = 0;
    int successes = 0;
    if (!bestSequence.empty()) {
        successes = 1;
        log << "[1] Start: score=" << std::fixed << std::setprecision(4) << bestScore
            << " len=" << bestSequence.size()
            << " seq=[" << bestSequence.toString() << "]\n";
    }

    // Phase 1: random exploration (50% of budget)
    int randomBudget = ctx.maxEvaluations / 2;
    for (int i = 0; i < randomBudget; ++i) {
        attempts++;
        int tryLen = 3 + (rng() % std::max(1, len - 3 + 1));
        OptSequence candidate = generator_->generateRandom(tryLen, rng);
        EvaluationResult evalResult = ctx.evaluator->evaluate(candidate);

        if (!evalResult.success) continue;
        if (evalResult.metrics.totalInstructions <= 0) continue;

        double score = ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
        if (score >= 1e8) continue;

        successes++;
        if (ctx.costModel->isBetter(score, bestScore)) {
            bestScore = score;
            bestSequence = candidate;
            bestMetrics = evalResult.metrics;
            log << "[" << successes << "] score=" << std::fixed << std::setprecision(4) << score
                << " len=" << candidate.size()
                << " seq=[" << candidate.toString() << "]\n";
        }
    }

    // Phase 2: exploit best by mutation (50% of budget)
    if (!bestSequence.empty()) {
        SequenceMutator mutator(registry_.get(), maxSequenceLength_);
        OptSequence current = bestSequence;
        double currentScore = bestScore;

        for (int i = 0; i < randomBudget && attempts < ctx.maxEvaluations; ++i) {
            attempts++;
            OptSequence neighbor = mutator.mutate(current, rng);
            EvaluationResult evalResult = ctx.evaluator->evaluate(neighbor);

            if (!evalResult.success) continue;
            if (evalResult.metrics.totalInstructions <= 0) continue;

            double score = ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
            if (score >= 1e8) continue;

            successes++;
            if (ctx.costModel->isBetter(score, currentScore)) {
                current = neighbor;
                currentScore = score;
                if (ctx.costModel->isBetter(score, bestScore)) {
                    bestScore = score;
                    bestSequence = neighbor;
                    bestMetrics = evalResult.metrics;
                    log << "[" << successes << "] score=" << std::fixed << std::setprecision(4) << score
                        << " len=" << neighbor.size()
                        << " seq=[" << neighbor.toString() << "]\n";
                }
            }
        }
    }

    if (successes == 0) {
        log << "No successful evaluations out of " << attempts << " attempts\n";
    } else {
        log << "Best: score=" << std::fixed << std::setprecision(4) << bestScore
            << " (" << successes << "/" << attempts << " ok)\n";
    }

    AlgoResult result;
    result.bestSequence = bestSequence;
    result.bestMetrics = bestMetrics;
    result.bestScore = bestScore;
    result.evaluationsUsed = successes;
    result.improvementRatio = 0.0;
    result.log = log.str();
    return result;
}

// 
// HillClimbing: start from a good sequence, mutate to improve
// 

HillClimbing::HillClimbing(int maxNoImprove, int maxSequenceLength)
    : maxNoImprove_(maxNoImprove)
    , maxSequenceLength_(maxSequenceLength)
    , registry_(std::make_unique<PassRegistry>())
    , generator_(std::make_unique<SequenceGenerator>(registry_.get()))
    , mutator_(std::make_unique<SequenceMutator>(registry_.get(), maxSequenceLength)) {}

AlgoResult HillClimbing::run(const AlgoContext& ctx) {
    std::random_device rd;
    std::mt19937 rng(rd());

    int effectiveMaxNoImprove = std::max(30, std::min(maxNoImprove_, ctx.maxEvaluations / 4));
    static const int MAX_RESTARTS = 3;

    OptSequence bestSequence;
    double bestScore = 1e9;
    IRMetrics bestMetrics;
    int totalAttempts = 0;
    int successes = 0;
    std::ostringstream log;
    int restarts = 0;

    while (totalAttempts < ctx.maxEvaluations && restarts <= MAX_RESTARTS) {
        // Try known good starting points on first iteration, random on restarts
        OptSequence current;
        double currentScore = 1e9;
        IRMetrics currentMetrics;

        if (restarts == 0) {
            OptSequence candidates[] = {
                generator_->generateO2(),
                generator_->generateO1(),
                generator_->generateRandom(3, rng)
            };
            for (auto& candidate : candidates) {
                EvaluationResult eval = ctx.evaluator->evaluate(candidate);
                if (eval.success && eval.metrics.totalInstructions > 0) {
                    double score = ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
                    if (score < 1e8 && ctx.costModel->isBetter(score, currentScore)) {
                        current = candidate;
                        currentScore = score;
                        currentMetrics = eval.metrics;
                    }
                }
            }
            if (current.empty()) {
                AlgoResult failResult;
                failResult.bestScore = 1e9;
                failResult.log = "All starting sequences failed\n";
                return failResult;
            }
            bestSequence = current;
            bestScore = currentScore;
            bestMetrics = currentMetrics;
            log << "Start: score=" << std::fixed << std::setprecision(4) << currentScore
                << " len=" << current.size()
                << " seq=[" << current.toString() << "]\n";
        } else {
            // Restart: shake the best sequence by applying multiple random mutations
            current = bestSequence;
            for (int shake = 0; shake < 5; ++shake) {
                current = mutator_->mutate(current, rng);
            }
            EvaluationResult eval = ctx.evaluator->evaluate(current);
            if (eval.success && eval.metrics.totalInstructions > 0) {
                currentScore = ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
                if (currentScore < 1e8) {
                    currentMetrics = eval.metrics;
                }
            }
            if (currentScore >= 1e8) {
                // Restart failed, generate random instead
                OptSequence randomSeq = generator_->generateRandom(
                    std::max(3, std::min(10, ctx.maxSequenceLength)), rng);
                EvaluationResult eval2 = ctx.evaluator->evaluate(randomSeq);
                if (eval2.success && eval2.metrics.totalInstructions > 0) {
                    currentScore = ctx.costModel->score(eval2.metrics, ctx.baselineMetrics);
                    if (currentScore < 1e8) {
                        current = randomSeq;
                        currentMetrics = eval2.metrics;
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
            log << "[Restart " << restarts << "] score=" << std::fixed << std::setprecision(4)
                << currentScore << " len=" << current.size() << "\n";
        }

        int noImproveCount = 0;
        while (noImproveCount < effectiveMaxNoImprove && totalAttempts < ctx.maxEvaluations) {
            OptSequence neighbor = mutator_->mutate(current, rng);
            totalAttempts++;

            EvaluationResult evalResult = ctx.evaluator->evaluate(neighbor);
            if (!evalResult.success) {
                noImproveCount++;
                continue;
            }
            if (evalResult.metrics.totalInstructions <= 0) {
                noImproveCount++;
                continue;
            }
            double neighborScore = ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
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
                    log << "[" << successes << "] New best: score=" << std::fixed
                        << std::setprecision(4) << bestScore
                        << " len=" << neighbor.size()
                        << " seq=[" << neighbor.toString() << "]\n";
                }
            } else {
                noImproveCount++;
            }
        }
        restarts++;
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

// 
// SimulatedAnnealing: like hill climbing but can accept worse
// solutions with decreasing probability
// 

SimulatedAnnealing::SimulatedAnnealing(double startTemperature, double coolingRate, int maxSequenceLength)
    : startTemperature_(startTemperature)
    , coolingRate_(coolingRate)
    , maxSequenceLength_(maxSequenceLength)
    , registry_(std::make_unique<PassRegistry>())
    , generator_(std::make_unique<SequenceGenerator>(registry_.get()))
    , mutator_(std::make_unique<SequenceMutator>(registry_.get(), maxSequenceLength)) {}

AlgoResult SimulatedAnnealing::run(const AlgoContext& ctx) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> probDist(0.0, 1.0);

    // Start from a known good sequence
    OptSequence current;
    double currentScore = 1e9;
    IRMetrics currentMetrics;

    OptSequence candidates[] = {
        generator_->generateO2(),
        generator_->generateO1(),
        generator_->generateRandom(
            std::min(3, ctx.maxSequenceLength > 0 ? ctx.maxSequenceLength : 3), rng)
    };

    for (auto& candidate : candidates) {
        EvaluationResult eval = ctx.evaluator->evaluate(candidate);
        if (eval.success && eval.metrics.totalInstructions > 0) {
            double score = ctx.costModel->score(eval.metrics, ctx.baselineMetrics);
            if (score < 1e8 && ctx.costModel->isBetter(score, currentScore)) {
                current = candidate;
                currentScore = score;
                currentMetrics = eval.metrics;
            }
        }
    }

    if (current.empty()) {
        AlgoResult result;
        result.bestScore = 1e9;
        result.log = "All starting sequences failed\n";
        return result;
    }

    OptSequence bestSequence = current;
    double bestScore = currentScore;
    IRMetrics bestMetrics = currentMetrics;

    double temperature = startTemperature_;
    int totalAttempts = 0;
    int successes = 0;
    std::ostringstream log;

    log << "Start: score=" << std::fixed << std::setprecision(4) << currentScore
        << " len=" << current.size()
        << " seq=[" << current.toString() << "]\n";

    while (totalAttempts < ctx.maxEvaluations && temperature > 0.01) {
        for (int i = 0; i < 5 && totalAttempts < ctx.maxEvaluations; ++i) {
            OptSequence neighbor = mutator_->mutate(current, rng);
            totalAttempts++;

            EvaluationResult evalResult = ctx.evaluator->evaluate(neighbor);
            if (!evalResult.success) continue;

            if (evalResult.metrics.totalInstructions <= 0) continue;

            double neighborScore = ctx.costModel->score(evalResult.metrics, ctx.baselineMetrics);
            if (neighborScore >= 1e8) continue;

            successes++;

            double delta = neighborScore - currentScore;
            if (delta < 0 || probDist(rng) < std::exp(-delta / temperature)) {
                current = neighbor;
                currentScore = neighborScore;
                currentMetrics = evalResult.metrics;

                if (ctx.costModel->isBetter(neighborScore, bestScore)) {
                    bestSequence = neighbor;
                    bestScore = neighborScore;
                    bestMetrics = evalResult.metrics;
                    log << "[" << successes << "] New best: score=" << std::fixed << std::setprecision(4)
                        << bestScore << " len=" << neighbor.size()
                        << " seq=[" << neighbor.toString() << "]\n";
                }
            }
        }
        temperature *= coolingRate_;
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

} // namespace phaseordering