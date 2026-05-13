#include <iostream>
#include <vector>
#include <chrono>
#include <cstdint>

int64_t sumNaive(int n) {
    int64_t result = 0;
    for (int i = 1; i <= n; i++) {
        result += i;
    }
    return result;
}

int64_t sumFormula(int n) {
    return static_cast<int64_t>(n) * (n + 1) / 2;
}

int64_t fibonacciNaive(int n) {
    if (n <= 1) return n;
    int64_t a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int64_t temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

int64_t matrixMultiply() {
    const int N = 50;
    std::vector<std::vector<int64_t>> A(N, std::vector<int64_t>(N, 1));
    std::vector<std::vector<int64_t>> B(N, std::vector<int64_t>(N, 2));
    std::vector<std::vector<int64_t>> C(N, std::vector<int64_t>(N, 0));

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return C[0][0];
}

int64_t nestedLoops() {
    int64_t count = 0;
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            for (int k = 0; k < 10; k++) {
                count += i + j + k;
            }
        }
    }
    return count;
}

int main() {
    const int N = 10000000;

    auto start = std::chrono::high_resolution_clock::now();
    volatile int64_t r1 = sumNaive(N);
    auto end = std::chrono::high_resolution_clock::now();
    double naiveMs = std::chrono::duration<double, std::milli>(end - start).count();

    start = std::chrono::high_resolution_clock::now();
    volatile int64_t r2 = sumFormula(N);
    end = std::chrono::high_resolution_clock::now();
    double formulaMs = std::chrono::duration<double, std::milli>(end - start).count();

    start = std::chrono::high_resolution_clock::now();
    volatile int64_t r3 = fibonacciNaive(1000000);
    end = std::chrono::high_resolution_clock::now();
    double fibMs = std::chrono::duration<double, std::milli>(end - start).count();

    start = std::chrono::high_resolution_clock::now();
    volatile int64_t r4 = matrixMultiply();
    end = std::chrono::high_resolution_clock::now();
    double matMs = std::chrono::duration<double, std::milli>(end - start).count();

    start = std::chrono::high_resolution_clock::now();
    volatile int64_t r5 = nestedLoops();
    end = std::chrono::high_resolution_clock::now();
    double nestMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "=== Compute Benchmark Results ===\n";
    std::cout << "Sum naive (" << N << " iterations): " << naiveMs << " ms\n";
    std::cout << "Sum formula: " << formulaMs << " ms\n";
    std::cout << "Fibonacci (1M iterations): " << fibMs << " ms\n";
    std::cout << "Matrix multiply (50x50): " << matMs << " ms\n";
    std::cout << "Nested loops: " << nestMs << " ms\n";
    std::cout << "Total time: " << naiveMs + formulaMs + fibMs + matMs + nestMs << " ms\n";

    return 0;
}