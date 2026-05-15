cmake_minimum_required(VERSION 3.25)
project(SysticSkeleton C CXX)

# Rename this variable when forking — it propagates to all target names below.
set(PROJECT_TARGET_PREFIX ${PROJECT_NAME})

# ---------------------------------------------------------------
# Language Standard
# Fixes the standard so the compiler cannot silently fall back
# to an older version and hide incompatibilities.
# ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------
# Compiler Guard
# This build is not tested against GCC or MSVC. Fail early
# rather than produce a binary with undefined behaviour from
# ABI or intrinsic mismatches.
# ---------------------------------------------------------------
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "This project requires Clang. Found: ${CMAKE_CXX_COMPILER_ID}. "
        "Update your Conan profile or set CMAKE_CXX_COMPILER explicitly."
    )
endif()

# ---------------------------------------------------------------
# Dependencies (provided by Conan via CMakeDeps generator)
# ---------------------------------------------------------------
find_package(GTest     REQUIRED)
find_package(benchmark REQUIRED)

include_directories(include)

# ---------------------------------------------------------------
# Static Analysis (optional)
# When clang-tidy is present it runs as part of the normal build,
# turning warnings into errors so analysis failures break the build
# rather than going unnoticed in a separate step.
# ---------------------------------------------------------------
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-21 clang-tidy-18)
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXE}
        -checks=*,-fuchsia-*,-llvmlibc-*
        --warnings-as-errors=*
    )
    message(STATUS "clang-tidy found at ${CLANG_TIDY_EXE} — static analysis active.")
endif()

# ---------------------------------------------------------------
# Core Library
# Compiled as a static library so tests, benchmarks, and any
# application entry point all link the same translation units
# without duplicating build rules.
# ---------------------------------------------------------------
add_library(${PROJECT_TARGET_PREFIX}_core STATIC src/core.cpp)

target_include_directories(${PROJECT_TARGET_PREFIX}_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------
# Test Executable
# ---------------------------------------------------------------
enable_testing()

add_executable(${PROJECT_TARGET_PREFIX}_tests tests/main_test.cpp)
target_link_libraries(${PROJECT_TARGET_PREFIX}_tests PRIVATE
    ${PROJECT_TARGET_PREFIX}_core
    GTest::gtest
    GTest::gtest_main
)

# ---------------------------------------------------------------
# Benchmark Executable
# ---------------------------------------------------------------
add_executable(${PROJECT_TARGET_PREFIX}_bench bench/main_bench.cpp)
target_link_libraries(${PROJECT_TARGET_PREFIX}_bench PRIVATE
    ${PROJECT_TARGET_PREFIX}_core
    benchmark::benchmark
    benchmark::benchmark_main
)

# ---------------------------------------------------------------
# Installation
# Installs the static library and public headers so the project
# can be consumed by downstream CMake projects via find_package.
# ---------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS ${PROJECT_TARGET_PREFIX}_core
    EXPORT   ${PROJECT_TARGET_PREFIX}Targets
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_TARGET_PREFIX}
)