#include "sequence_ops.hpp"

#include <algorithm>
#include <random>

namespace phaseordering {

PassRegistry::PassRegistry() {
    availablePasses_ = {"mem2reg",
                        "instcombine",
                        "simplifycfg",
                        "gvn",
                        "licm",
                        "loop-unroll",
                        "loop-rotate",
                        "reassociate",
                        "indvars",
                        "loop-simplify",
                        "loop-deletion",
                        "adce",
                        "dce",
                        "dse",
                        "sroa",
                        "early-cse",
                        "jump-threading",
                        "correlated-propagation",
                        "tailcallelim",
                        "loop-vectorize",
                        "slp-vectorizer",
                        "inline",
                        "globalopt",
                        "globaldce",
                        "constantmerge",
                        "deadargelim",
                        "speculative-execution",
                        "lower-expect",
                        "mergefunc",
                        "sink",
                        "instsimplify",
                        "bdce",
                        "lcssa",
                        "elim-avail-extern",
                        "function-attrs",
                        "always-inline",
                        "prune-eh",
                        "div-rem-pairs",
                        "gvn-hoist",
                        "mldst-motion",
                        "partially-inline-libcalls",
                        "reg2mem",
                        "unreachableblockelim",
                        "loop-predication",
                        "loop-distribute",
                        "loop-versioning",
                        "loop-load-elimination",
                        "scalar-evolution",
                        "break-crit-edges"};
}

const std::vector<std::string>& PassRegistry::allPasses() const {
    return availablePasses_;
}

std::string PassRegistry::randomPass(std::mt19937& rng) const {
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(availablePasses_.size()) - 1);
    return availablePasses_[dist(rng)];
}

bool PassRegistry::contains(const std::string& pass) const {
    return std::find(availablePasses_.begin(), availablePasses_.end(), pass) !=
           availablePasses_.end();
}

SequenceGenerator::SequenceGenerator(const PassRegistry* registry)
    : registry_(registry) {}

OptSequence SequenceGenerator::generateRandom(int length,
                                              std::mt19937& rng) const {
    OptSequence seq;
    for (int i = 0; i < length; ++i) {
        seq.add(registry_->randomPass(rng));
    }
    return seq;
}

OptSequence SequenceGenerator::generateO1() const {
    return OptSequence({"mem2reg", "simplifycfg", "instcombine"});
}

OptSequence SequenceGenerator::generateO2() const {
    return OptSequence({"mem2reg", "simplifycfg", "instcombine", "reassociate",
                        "loop-simplify", "licm", "simplifycfg", "instcombine",
                        "gvn", "sroa", "adce"});
}

OptSequence SequenceGenerator::generateO3() const {
    return OptSequence({"mem2reg", "simplifycfg", "instcombine", "reassociate",
                        "loop-simplify", "licm", "loop-unroll", "simplifycfg",
                        "instcombine", "gvn", "sroa", "adce", "loop-vectorize",
                        "slp-vectorizer", "instcombine", "simplifycfg"});
}

OptSequence SequenceGenerator::generateEmpty() const { return OptSequence(); }

OptSequence SequenceGenerator::generateFromMetrics(const IRMetrics& metrics,
                                                   int length,
                                                   std::mt19937& rng) const {
    OptSequence seq;
    for (int i = 0; i < length; ++i) {
        if (metrics.memoryOps > metrics.arithmeticOps) {
            std::vector<std::string> memPasses = {"mem2reg", "sroa", "licm",
                                                  "gvn", "early-cse"};
            std::uniform_int_distribution<int> dist(
                0, static_cast<int>(memPasses.size()) - 1);
            if (i % 3 == 0) {
                seq.add(memPasses[dist(rng)]);
            } else {
                seq.add(registry_->randomPass(rng));
            }
        } else if (metrics.branchOps > metrics.arithmeticOps) {
            std::vector<std::string> branchPasses = {
                "simplifycfg", "jump-threading", "correlated-propagation",
                "instcombine"};
            std::uniform_int_distribution<int> dist(
                0, static_cast<int>(branchPasses.size()) - 1);
            if (i % 3 == 0) {
                seq.add(branchPasses[dist(rng)]);
            } else {
                seq.add(registry_->randomPass(rng));
            }
        } else {
            seq.add(registry_->randomPass(rng));
        }
    }
    return seq;
}

SequenceMutator::SequenceMutator(const PassRegistry* registry, int maxLength)
    : registry_(registry), maxLength_(maxLength) {}

OptSequence SequenceMutator::mutate(const OptSequence& seq,
                                    std::mt19937& rng) const {
    if (seq.empty()) {
        return insertRandomPass(seq, rng);
    }

    std::uniform_int_distribution<int> dist(0, 3);
    int mutation = dist(rng);

    switch (mutation) {
        case 0:
            return insertRandomPass(seq, rng);
        case 1:
            return removeRandomPass(seq, rng);
        case 2:
            return swapRandomPasses(seq, rng);
        case 3:
            return changeRandomPass(seq, rng);
        default:
            return changeRandomPass(seq, rng);
    }
}

OptSequence SequenceMutator::insertRandomPass(const OptSequence& seq,
                                              std::mt19937& rng) const {
    if (seq.size() >= maxLength_) {
        return changeRandomPass(seq, rng);
    }

    OptSequence result = seq;
    std::string pass = registry_->randomPass(rng);

    if (result.empty()) {
        result.add(pass);
    } else {
        std::uniform_int_distribution<int> posDist(0, result.size());
        int pos = posDist(rng);
        result.passes().insert(result.passes().begin() + pos, pass);
    }
    return result;
}

OptSequence SequenceMutator::removeRandomPass(const OptSequence& seq,
                                              std::mt19937& rng) const {
    if (seq.empty()) return seq;

    OptSequence result = seq;
    std::uniform_int_distribution<int> dist(0, result.size() - 1);
    result.removeAt(dist(rng));
    return result;
}

OptSequence SequenceMutator::swapRandomPasses(const OptSequence& seq,
                                              std::mt19937& rng) const {
    if (seq.size() < 2) return seq;

    OptSequence result = seq;
    auto& passes = result.passes();

    std::uniform_int_distribution<int> dist(0, result.size() - 1);
    int i = dist(rng);
    int j = dist(rng);
    int attempts = 0;
    while (j == i && attempts < 10) {
        j = dist(rng);
        attempts++;
    }
    if (j == i) return seq;

    std::swap(passes[i], passes[j]);
    return result;
}

OptSequence SequenceMutator::changeRandomPass(const OptSequence& seq,
                                              std::mt19937& rng) const {
    if (seq.empty()) return seq;

    OptSequence result = seq;
    auto& passes = result.passes();

    std::uniform_int_distribution<int> dist(0, result.size() - 1);
    int i = dist(rng);
    passes[i] = registry_->randomPass(rng);
    return result;
}

}  // namespace phaseordering
