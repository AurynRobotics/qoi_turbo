cmake_minimum_required(VERSION 3.20)
project(qoi_turbo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Header-only library ----

add_library(qoi_turbo INTERFACE)
add_library(qoi_turbo::qoi_turbo ALIAS qoi_turbo)
target_include_directories(qoi_turbo INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(qoi_turbo INTERFACE cxx_std_17)

# ---- Options (default ON only when we're the top-level project) ----

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(QOI_TURBO_IS_TOPLEVEL ON)
else()
    set(QOI_TURBO_IS_TOPLEVEL OFF)
endif()

option(QOI_TURBO_BUILD_TESTS "Build qoi_turbo correctness tests" ${QOI_TURBO_IS_TOPLEVEL})
option(QOI_TURBO_BUILD_BENCH "Build qoi_turbo benchmark"         ${QOI_TURBO_IS_TOPLEVEL})

# ---- Tests (uses phoboslab/qoi as the reference codec) ----

if(QOI_TURBO_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        phoboslab_qoi
        GIT_REPOSITORY https://github.com/phoboslab/qoi.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(phoboslab_qoi)

    add_executable(qoi_turbo_test test/test_qoi_turbo.cpp)
    target_link_libraries(qoi_turbo_test PRIVATE qoi_turbo)
    target_include_directories(qoi_turbo_test PRIVATE ${phoboslab_qoi_SOURCE_DIR})
    target_compile_options(qoi_turbo_test PRIVATE -O3 -march=native)

    enable_testing()
    add_test(NAME qoi_turbo_test COMMAND qoi_turbo_test)
endif()

# ---- Benchmark (uses google/benchmark) ----

if(QOI_TURBO_BUILD_BENCH)
    include(FetchContent)
    FetchContent_Declare(
        phoboslab_qoi_bench
        GIT_REPOSITORY https://github.com/phoboslab/qoi.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(phoboslab_qoi_bench)

    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(qoi_turbo_bench bench/bench_qoi_turbo.cpp)
    target_link_libraries(qoi_turbo_bench PRIVATE qoi_turbo benchmark::benchmark_main)
    target_include_directories(qoi_turbo_bench PRIVATE ${phoboslab_qoi_bench_SOURCE_DIR})
    target_compile_options(qoi_turbo_bench PRIVATE -O3 -march=native)
endif()
