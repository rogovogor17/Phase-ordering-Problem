#include <gtest/gtest.h>
#include "sequence_ops.hpp"

using namespace phaseordering;

TEST(PassRegistryTest, NonEmpty) {
    PassRegistry registry;
    EXPECT_FALSE(registry.allPasses().empty());
}

TEST(PassRegistryTest, ContainsKnownPass) {
    PassRegistry registry;
    EXPECT_TRUE(registry.contains("mem2reg"));
    EXPECT_TRUE(registry.contains("instcombine"));
    EXPECT_TRUE(registry.contains("simplifycfg"));
}

TEST(PassRegistryTest, DoesNotContainUnknownPass) {
    PassRegistry registry;
    EXPECT_FALSE(registry.contains("nonexistent_pass_xyz"));
}

TEST(PassRegistryTest, RandomPassReturnsValidPass) {
    PassRegistry registry;
    std::mt19937 rng(42);
    for (int i = 0; i < 100; ++i) {
        std::string pass = registry.randomPass(rng);
        EXPECT_TRUE(registry.contains(pass));
    }
}

TEST(SequenceGeneratorTest, GenerateRandom) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    std::mt19937 rng(42);

    auto seq = gen.generateRandom(5, rng);
    EXPECT_EQ(seq.size(), 5);
    EXPECT_FALSE(seq.empty());
}

TEST(SequenceGeneratorTest, GenerateRandomLength) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    std::mt19937 rng(42);

    for (int len = 1; len <= 20; ++len) {
        auto seq = gen.generateRandom(len, rng);
        EXPECT_EQ(seq.size(), len);
    }
}

TEST(SequenceGeneratorTest, GenerateO1) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    auto seq = gen.generateO1();
    EXPECT_FALSE(seq.empty());
    EXPECT_GT(seq.size(), 0);
}

TEST(SequenceGeneratorTest, GenerateO2) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    auto seq = gen.generateO2();
    EXPECT_FALSE(seq.empty());
    EXPECT_GT(seq.size(), 0);
}

TEST(SequenceGeneratorTest, GenerateO3) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    auto seq = gen.generateO3();
    EXPECT_FALSE(seq.empty());
    EXPECT_GT(seq.size(), 0);
}

TEST(SequenceGeneratorTest, GenerateEmpty) {
    PassRegistry registry;
    SequenceGenerator gen(&registry);
    auto seq = gen.generateEmpty();
    EXPECT_TRUE(seq.empty());
    EXPECT_EQ(seq.size(), 0);
}

TEST(SequenceMutatorTest, InsertRandomPass) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    seq.add("mem2reg");
    auto result = mut.insertRandomPass(seq, rng);
    EXPECT_EQ(result.size(), 2);
}

TEST(SequenceMutatorTest, InsertRandomPassIntoEmpty) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    auto result = mut.insertRandomPass(seq, rng);
    EXPECT_EQ(result.size(), 1);
}

TEST(SequenceMutatorTest, RemoveRandomPass) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    seq.add("mem2reg");
    seq.add("instcombine");
    seq.add("simplifycfg");
    auto result = mut.removeRandomPass(seq, rng);
    EXPECT_EQ(result.size(), 2);
}

TEST(SequenceMutatorTest, RemoveFromEmpty) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    auto result = mut.removeRandomPass(seq, rng);
    EXPECT_TRUE(result.empty());
}

TEST(SequenceMutatorTest, SwapRandomPasses) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    seq.add("mem2reg");
    seq.add("instcombine");
    auto result = mut.swapRandomPasses(seq, rng);
    EXPECT_EQ(result.size(), 2);
}

TEST(SequenceMutatorTest, SwapSinglePassSequence) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    seq.add("mem2reg");
    auto result = mut.swapRandomPasses(seq, rng);
    EXPECT_EQ(result.size(), 1);
}

TEST(SequenceMutatorTest, MutateAddsModification) {
    PassRegistry registry;
    SequenceMutator mut(&registry);
    std::mt19937 rng(42);

    OptSequence seq;
    seq.add("mem2reg");
    auto result = mut.mutate(seq, rng);
    EXPECT_NE(result.size(), seq.size());
}