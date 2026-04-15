include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.4
    GIT_SHALLOW TRUE
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_BUILD_32_BITS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_LTO OFF CACHE BOOL "" FORCE)
set(BENCHMARK_USE_LIBCXX OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googlebenchmark)

file(GLOB_RECURSE BENCHMARK_FILES CONFIGURE_DEPENDS "bench_*.cpp")

foreach(BENCHMARK_FILE ${BENCHMARK_FILES})
    get_filename_component(BENCHMARK_NAME ${BENCHMARK_FILE} NAME_WE)

    add_executable(${BENCHMARK_NAME} ${BENCHMARK_FILE})
    target_include_directories(${BENCHMARK_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_options(${BENCHMARK_NAME} PRIVATE -O3 -DNDEBUG)

    target_link_libraries(${BENCHMARK_NAME}
        PRIVATE benchmark::benchmark
        PRIVATE benchmark::benchmark_main
        PRIVATE implementation
    )
endforeach()
