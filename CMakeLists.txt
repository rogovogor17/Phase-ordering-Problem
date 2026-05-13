cmake_minimum_required(VERSION 3.14)
project(PhaseOrdering VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)

add_library(phaseordering_lib
    src/data_types.cpp
    # src/logging.cpp
    src/sequence_ops.cpp
    src/ir_analyzer.cpp
    src/llvm_facade.cpp
    # src/evaluator_impl.cpp
    # src/algorithms.cpp
    # src/cost_models.cpp
    # src/factories.cpp
    src/solver.cpp
)

target_include_directories(phaseordering_lib PUBLIC include)
target_link_libraries(phaseordering_lib PRIVATE Threads::Threads)

add_executable(phaseordering src/main.cpp)
target_link_libraries(phaseordering PRIVATE phaseordering_lib)

include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(phaseordering_bench
    benchmark/benchmark_main.cpp
)
target_link_libraries(phaseordering_bench PRIVATE phaseordering_lib benchmark::benchmark)