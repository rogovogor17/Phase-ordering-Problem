#pragma once

#include <random>
#include <string>
#include <vector>

#include "data_types.hpp"

namespace phaseordering {

/**
 * Registry of available LLVM optimization passes.
 *
 * Central place for pass names. Prevents hardcoding pass
 * lists inside algorithms.
 */
class PassRegistry {
   private:
    std::vector<std::string> availablePasses_;

   public:
    PassRegistry();
    const std::vector<std::string>& allPasses() const;
    std::string randomPass(std::mt19937& rng) const;
    bool contains(const std::string& pass) const;
};

/**
 * Creates initial candidate optimization sequences.
 *
 * Used by all algorithms to produce starting points.
 * Does not evaluate sequences.
 */
class SequenceGenerator {
   private:
    const PassRegistry* registry_;

   public:
    explicit SequenceGenerator(const PassRegistry* registry);  // NOTE

    /** Generate a random sequence of the given length. */
    OptSequence generateRandom(int length, std::mt19937& rng) const;
    /** Generate a sequence mimicking -O1. */
    OptSequence generateO1() const;
    /** Generate a sequence mimicking -O2. */
    OptSequence generateO2() const;
    /** Generate a sequence mimicking -O3. */
    OptSequence generateO3() const;
    /** Generate an empty sequence (no passes). */
    OptSequence generateEmpty() const;
    /** Generate a heuristically-informed sequence based on initial IR metrics.
     */
    OptSequence generateFromMetrics(const IRMetrics& metrics, int length,
                                    std::mt19937& rng) const;
};

/**
 * Produces neighbor sequences from a given sequence.
 *
 * Useful for local-search algorithms: HillClimbing,
 * SimulatedAnnealing, and genetic algorithms later.
 * Respects maxLength to prevent unbounded sequence growth.
 */
class SequenceMutator {
   private:
    const PassRegistry* registry_;
    int maxLength_;

   public:
    explicit SequenceMutator(const PassRegistry* registry, int maxLength = 100);

    /** Apply a random mutation (insert, remove, swap, or change). */
    OptSequence mutate(const OptSequence& seq, std::mt19937& rng) const;
    /** Insert a random pass at a random position. */
    OptSequence insertRandomPass(const OptSequence& seq,
                                 std::mt19937& rng) const;
    /** Remove a pass at a random position. */
    OptSequence removeRandomPass(const OptSequence& seq,
                                 std::mt19937& rng) const;
    /** Swap two random passes. */
    OptSequence swapRandomPasses(const OptSequence& seq,
                                 std::mt19937& rng) const;
    /** Replace a random pass with a different random pass. */
    OptSequence changeRandomPass(const OptSequence& seq,
                                 std::mt19937& rng) const;
};

}  // namespace phaseordering
