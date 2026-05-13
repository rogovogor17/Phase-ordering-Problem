# Phase-Ordering Problem Solver

Educational C++20 project for solving the LLVM Phase-Ordering Problem using optimization search algorithms.

## Build Requirements

- CMake 3.14+
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 19.28+)
- LLVM 17+ (clang, opt, lli) installed and available in PATH or specified via `--llvm-path`

## Build Instructions

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

```bash
./phaseordering --source test.cpp [options]
./phaseordering --ir input.ll [options]
```

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `--source <file>` | Source C/C++ file to optimize | - |
| `--ir <file>` | Input LLVM IR file | - |
| `--algorithm <type>` | Algorithm: `random`, `hillclimbing`, `sa` | `hillclimbing` |
| `--costmodel <type>` | Cost model: `weighted`, `instructions`, `runtime` | `weighted` |
| `--max-eval <N>` | Maximum evaluations | 100 |
| `--seq-len <N>` | Sequence length | 5 |
| `--measure-runtime` | Enable runtime measurement | off |
| `--llvm-path <path>` | Path to LLVM bin directory | system PATH |
| `--save-initial-ir` | Save initial IR to file | off |
| `--save-final-ir` | Save optimized IR to file | off |
| `--metrics-comparison` | Print metrics comparison | on |
| `--runtime-comparison` | Print runtime comparison | on |
| `--print-progress` | Print progress messages | off |
| `--print-each-eval` | Print each evaluation result | off |
| `--output-dir <dir>` | Output directory for logs and IR | `.` |
| `--verbose` | Enable verbose mode | off |

### Example

```bash
./phaseordering --source comp_test.cpp --algorithm hillclimbing --max-eval 50 --verbose
```

## Architecture

The project follows Strategy, Facade, Factory, and Builder design patterns:

- **IAlgorithm** — Strategy interface for search algorithms (RandomSearch, HillClimbing, SimulatedAnnealing)
- **ICostModel** — Strategy for optimization objective (WeightedCostModel, InstructionCountCostModel, RuntimeCostModel)
- **IEvaluator** — Abstraction for evaluating optimization sequences
- **LLVMFacade** — Facade for LLVM CLI operations
- **Factory classes** — Create algorithms, cost models, and evaluators
- **SolverBuilder** — Builder pattern for PhaseOrderingSolver configuration

## Running Tests

```bash
cd build
ctest
```

Or directly:

```bash
./phaseordering_test
```

## Running Benchmarks

```bash
./phaseordering_bench
```

## Project Structure

```
├── CMakeLists.txt
├── include/           # Header files
│   ├── algorithms.hpp
│   ├── cost_models.hpp
│   ├── data_types.hpp
│   ├── evaluator_impl.hpp
│   ├── factories.hpp
│   ├── interfaces.hpp
│   ├── ir_analyzer.hpp
│   ├── logging.hpp
│   ├── llvm_facade.hpp
│   ├── sequence_ops.hpp
│   └── solver.hpp
├── src/               # Implementation files
├── test/              # Google Test files
├── benchmark/         # Google Benchmark files
├── assets/            # Diagrams and resources
└── comp_test.cpp      # Test program for compilation checking
```