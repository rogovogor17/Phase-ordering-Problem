#pragma once

#include <memory>
#include <random>

#include "interfaces.hpp"
#include "sequence_ops.hpp"

namespace phaseordering {

/**
 * Random search: generates random sequences and keeps the best one.
 *
 * @see IAlgorithm
 */
class RandomSearch : public IAlgorithm {
   private:
    int sequenceLength_;
    int maxSequenceLength_;
    std::unique_ptr<PassRegistry> registry_;
    std::unique_ptr<SequenceGenerator> generator_;

   public:
    /**
     * @param sequenceLength Length of each random candidate.
     * @param maxSequenceLength Maximum allowed sequence length.
     */
    RandomSearch(int sequenceLength, int maxSequenceLength);
    AlgoResult run(const AlgoContext& ctx) override;
};

/**
 * Hill climbing: start from a random sequence, mutate, keep improvements.
 *
 * Stops after maxNoImprove consecutive evaluations without improvement.
 *
 * @see IAlgorithm
 */
class HillClimbing : public IAlgorithm {
   private:
    int maxNoImprove_;
    int maxSequenceLength_;
    std::unique_ptr<PassRegistry> registry_;
    std::unique_ptr<SequenceGenerator> generator_;
    std::unique_ptr<SequenceMutator> mutator_;

   public:
    /**
     * @param maxNoImprove Stop after this many evaluations without improvement.
     * @param maxSequenceLength Maximum allowed sequence length.
     */
    HillClimbing(int maxNoImprove, int maxSequenceLength);
    AlgoResult run(const AlgoContext& ctx) override;
};

/**
 * Simulated annealing: probabilistically accepts worse solutions
 * early on, gradually reducing the acceptance probability.
 *
 * @see IAlgorithm
 */
class SimulatedAnnealing : public IAlgorithm {
   private:
    double startTemperature_;
    double coolingRate_;
    int maxSequenceLength_;
    std::unique_ptr<PassRegistry> registry_;
    std::unique_ptr<SequenceGenerator> generator_;
    std::unique_ptr<SequenceMutator> mutator_;

   public:
    /**
     * @param startTemperature Initial temperature (higher = more exploration).
     * @param coolingRate Factor by which temperature is multiplied each step
     * (e.g. 0.95).
     * @param maxSequenceLength Maximum allowed sequence length.
     */
    SimulatedAnnealing(double startTemperature, double coolingRate,
                       int maxSequenceLength);
    AlgoResult run(const AlgoContext& ctx) override;
};

}  // namespace phaseordering
