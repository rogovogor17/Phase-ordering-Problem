#include <benchmark/benchmark.h>

#include <random>

#include "utils.hpp"

static int rand_int(int l, int r) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(l, r);
    return dist(gen);
}

static void BM_Add(benchmark::State& state) {
    for (auto _ : state) {
        int a = rand_int(-100, 100);
        int b = rand_int(-100, 100);
        auto result = add(a, b);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Add);

static void BM_Mul(benchmark::State& state) {
    for (auto _ : state) {
        int a = rand_int(-100, 100);
        int b = rand_int(-100, 100);
        auto result = mul(a, b);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Mul);
