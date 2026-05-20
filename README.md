<!-- markdownlint-disable MD033 -->
<!-- markdownlint-disable MD041 -->

<a id="readme-top"></a>

[![C++][cpp-shield]][cpp-url]
[![LLVM][llvm-shield]][llvm-url]
[![CMake][cmake-shield]][cmake-url]
[![Benchmark][benchmark-shield]][benchmark-url]
[![Tests][tests-shield]][tests-url]

<br />
<div align="center">
  <img src="https://raw.githubusercontent.com/tandpfun/skill-icons/main/icons/CPP.svg" alt="Logo" width="80" height="80">

  <h3 align="center">Phase-Ordering Problem Solver</h3>
  <h4 align="center">LLVM Optimization Sequence Search Framework</h4>

<div align="center">

**Automatic LLVM Pass Sequence Optimization Using Heuristic Search Algorithms**

[Getting Started](#getting-started) • [Architecture](#architecture) • [Algorithms](#implemented-methods) • [Benchmarking](#benchmarking)

</div>
</div>

<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#key-features">Key Features</a></li>
      </ul>
    </li>
    <li><a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
        <li><a href="#building">Building</a></li>
      </ul>
    </li>
    <li><a href="#architecture">Architecture</a>
      <ul>
        <li><a href="#module-structure">Module Structure</a></li>
      </ul>
    </li>
    <li><a href="#implemented-methods">Implemented Methods</a></li>
    <li><a href="#testing">Testing</a></li>
    <li><a href="#benchmarking">Benchmarking</a></li>
    <li><a href="#performance-analysis">Performance Analysis</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
  </ol>
</details>

---

\anchor about-the-project

## About The Project

**Phase-Ordering Problem Solver** is a research-oriented C++20 framework for solving the classical compiler phase-ordering problem using LLVM infrastructure.

Modern compilers apply dozens of optimization passes:

```text
mem2reg → instcombine → gvn → simplifycfg → licm → dce
```

The final generated code quality strongly depends on the order of these passes.

This project automatically:

- Compiles C/C++ programs into LLVM IR
- Generates optimization pass sequences
- Applies LLVM optimization pipelines
- Measures IR quality metrics
- Searches for the best optimization sequence

The framework integrates directly with LLVM toolchain components such as `clang`, `opt`, `llvm-link`, and `lli`.

\anchor key-features

### Key Features

| Feature              | Description                                       |
| -------------------- | ------------------------------------------------- |
| LLVM Integration     | Uses real LLVM optimization passes                |
| Heuristic Search     | Random Search, Hill Climbing, Simulated Annealing |
| Cost Models          | Weighted, instruction-count, runtime-based        |
| Experiment Framework | Batch experiments and repeated runs               |
| CSV Export           | Stores optimization results                       |
| Parallel Execution   | Multi-threaded experiment runner                  |
| Testing              | Google Test integration                           |
| Benchmarking         | Google Benchmark support                          |
| Modular Design       | Easily extensible architecture                    |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor getting-started

## Getting Started

\anchor prerequisites

### Prerequisites

| Tool                 | Version       | Installation Command (Ubuntu)       |
| -------------------- | ------------- | ----------------------------------- |
| **CMake**            | ≥ 3.16        | `sudo apt install cmake`            |
| **LLVM**             | ≥ 14          | `sudo apt install llvm clang`       |
| **C++ Compiler**     | C++20 capable | GCC 12+, Clang 14+                  |
| **Git**              | Latest        | `sudo apt install git`              |
| **GoogleTest**       | Latest        | `sudo apt install libgtest-dev`     |
| **Google Benchmark** | Latest        | `sudo apt install libbenchmark-dev` |
| **Doxygen**          | Latest        | `sudo apt install doxygen`          |

\anchor installation

### Installation

```bash
# Clone repository
git clone https://github.com/your_username/phase-ordering-solver.git

cd phase-ordering-solver
```

\anchor building

### Building

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j
```

\anchor running

### Running

```bash
# Single source optimization
./bin/phaseordering --source test.cpp

# Run tests
ctest --output-on-failure

# Run benchmark
./benchmark/benchmark_main
```

### Simulated Annealing Example

```bash
./bin/phaseordering \
  --source test.cpp \
  --algorithm sa \
  --max-eval 300 \
  --seq-len 25
```

### Batch Experiment Example

```bash
./bin/phaseordering \
  --sources-list ../test.txt \
  --algorithm sa,hc \
  --costmodel weighted,instructions \
  --max-eval 100,300 \
  --seq-len 10,25 \
  --repeat 3 \
  --jobs 4 \
  --csv results.csv
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor architecture

## Architecture

\anchor module-structure

### Module Structure

| File / Module        | Description                      |
| -------------------- | -------------------------------- |
| `solver.*`           | Main experiment orchestration    |
| `algorithms.*`       | Search algorithms implementation |
| `interfaces.hpp`     | Core interfaces                  |
| `cost_models.*`      | Optimization scoring models      |
| `llvm_facade.*`      | LLVM toolchain wrapper           |
| `ir_analyzer.*`      | LLVM IR metrics extraction       |
| `sequence_ops.*`     | LLVM optimization pass registry  |
| `experiment_stats.*` | CSV/log export and statistics    |

### High-Level Pipeline

```text
C/C++ Source
      ↓
clang → LLVM IR
      ↓
Optimization Algorithm
      ↓
LLVM opt Pass Sequence
      ↓
Optimized IR
      ↓
IR Analyzer
      ↓
Cost Model
      ↓
Best Sequence Selection
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor implemented-methods

## Implemented Methods

### Random Search

Generates random optimization sequences and keeps the best result.

### Hill Climbing

Starts from a candidate solution and iteratively improves it using mutations.

### Simulated Annealing

Uses probabilistic acceptance to escape local minima and explore larger search spaces.

### Cost Models

| Model                       | Objective                  |
| --------------------------- | -------------------------- |
| `InstructionCountCostModel` | Minimize instruction count |
| `WeightedCostModel`         | Weighted IR metrics        |
| `RuntimeCostModel`          | Minimize execution time    |

### Supported LLVM Passes

Examples:

```text
mem2reg
instcombine
gvn
licm
simplifycfg
loop-unroll
dce
adce
sroa
reassociate
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor testing

## Testing

\anchor test-coverage

### Test Coverage

| Test Suite        | Description                  | Status |
| ----------------- | ---------------------------- | ------ |
| Algorithm tests   | Search algorithm correctness | Pass   |
| Cost model tests  | Score computation            | Pass   |
| IR analyzer tests | Metric extraction            | Pass   |
| Solver tests      | Experiment orchestration     | Pass   |

\anchor running-tests

### Running Tests

```bash
cd build
ctest --output-on-failure
```

\anchor test-results

### Test Results

```text
100% tests passed
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor benchmarking

## Benchmarking

\anchor benchmarked-on

### Benchmarked On

<table>
  <tr>
    <th>Component</th>
    <th>Specification</th>
  </tr>
  <tr>
    <td><b>CPU</b></td>
    <td>Intel / AMD x86_64</td>
  </tr>
  <tr>
    <td><b>Architecture</b></td>
    <td>x86_64<br><i>Linux</i></td>
  </tr>
  <tr>
    <td><b>Cache Hierarchy</b></td>
    <td>L1/L2/L3 CPU cache hierarchy</td>
  </tr>
  <tr>
    <td><b>Memory</b></td>
    <td>DDR4 / DDR5</td>
  </tr>
  <tr>
    <td><b>SIMD Extensions</b></td>
    <td>SSE / AVX / AVX2</td>
  </tr>
  <tr>
    <td><b>OS & Compiler</b></td>
    <td>Ubuntu Linux<br>GCC 13+, Clang 18+</td>
  </tr>
</table>

\anchor test-configurations

### Test Configuration

| Parameter       | Value        |
| --------------- | ------------ |
| Sequence Length | 5–80         |
| Evaluations     | 50–400       |
| Algorithms      | RS / HC / SA |
| LLVM Version    | LLVM 14+     |

### Example Experiment

```bash
./bin/phaseordering \
  --sources-list ../test.txt \
  --algorithm sa \
  --max-eval 400 \
  --seq-len 80 \
  --tee-log experiment.txt \
  --csv results.csv
```

### Output Example

```text
Best sequence:
mem2reg → instcombine → gvn → simplifycfg

Instruction reduction:
235 → 198
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor performance-analysis

## Performance Analysis

The project demonstrates how heuristic optimization methods can improve compiler optimization pipelines.

Key observations:

- LLVM optimization order significantly affects resulting IR quality
- Simulated Annealing performs better on large search spaces
- Hill Climbing converges faster but may get stuck in local minima
- Weighted cost models provide more balanced optimization behavior

The framework is useful for:

- compiler research
- optimization experimentation
- LLVM studies
- heuristic algorithm evaluation

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

\anchor roadmap

## Roadmap

- [x] **Phase 1: Core Framework**
  - [x] LLVM integration
  - [x] Optimization sequence execution
  - [x] IR analysis

- [x] **Phase 2: Search Algorithms**
  - [x] Random Search
  - [x] Hill Climbing
  - [x] Simulated Annealing

- [x] **Phase 3: Testing & Benchmarking**
  - [x] Google Benchmark integration
  - [x] Google Test suite
  - [x] CSV export
  - [x] Parallel experiments

- [ ] **Phase 4: Future Enhancements**
  - [ ] Genetic Algorithms
  - [ ] Reinforcement Learning
  - [ ] ML-guided optimization
  - [ ] Distributed experiments
  - [ ] LLVM New Pass Manager support

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

## References

1. LLVM Documentation — https://llvm.org/docs/
2. Engineering a Compiler — Keith Cooper & Linda Torczon
3. LLVM Passes Reference — https://llvm.org/docs/Passes.html

<p align="right">(<a href="#readme-top">back to top</a>)</p>

---

<div align="center">
  <sub>© 2026 — Phase-Ordering Problem Solver</sub>
</div>

[cpp-shield]: https://img.shields.io/badge/C++-20-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white
[cpp-url]: https://isocpp.org/
[llvm-shield]: https://img.shields.io/badge/LLVM-14+-262D3A?style=for-the-badge&logo=llvm&logoColor=white
[llvm-url]: https://llvm.org/
[cmake-shield]: https://img.shields.io/badge/CMake-3.16+-064F8C?style=for-the-badge&logo=cmake&logoColor=white
[cmake-url]: https://cmake.org/
[benchmark-shield]: https://img.shields.io/badge/Benchmark-Google-4285F4?style=for-the-badge&logo=google&logoColor=white
[benchmark-url]: https://github.com/google/benchmark
[tests-shield]: https://img.shields.io/badge/Tests-Google_Test-4285F4?style=for-the-badge&logo=google&logoColor=white
[tests-url]: https://github.com/google/googletest
