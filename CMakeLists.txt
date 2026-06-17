cmake_minimum_required(VERSION 3.25)
project(ArraySlotThreadSafe VERSION 1.0.0 LANGUAGES CXX)

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
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# ---------------------------------------------------------------
# Compiler Guard
# This build is not tested against GCC or MSVC. Fail early
# rather than produce a binary with undefined behaviour from
# ABI or intrinsic mismatches.
# ---------------------------------------------------------------
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
            "This project requires Clang. Found: ${CMAKE_CXX_COMPILER_ID}. "
            "Update your Conan profile or set CMAKE_CXX_COMPILER explicitly."
    )
endif ()

# ---------------------------------------------------------------
# Dependencies (provided by Conan via CMakeDeps generator)
# ---------------------------------------------------------------
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

include_directories(include)

# ---------------------------------------------------------------
# Core Library
# Compiled as a static library so tests, benchmarks, and any
# application entry point all link the same translation units
# without duplicating build rules.
# ---------------------------------------------------------------
add_library(${PROJECT_TARGET_PREFIX})

target_sources(
        ${PROJECT_TARGET_PREFIX}
        PUBLIC
        FILE_SET CXX_MODULES FILES
        src/ThreadSafe.ixx     # Module interface
        PRIVATE
        src/SlotThreadSafe.tpp    # Implementation
)

target_include_directories(
        ${PROJECT_TARGET_PREFIX} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------
# Test Executable
# ---------------------------------------------------------------
enable_testing()

add_executable(${PROJECT_TARGET_PREFIX}_tests
        tests/slot/SlotThreadSafeStressTest.cpp
        tests/slot/SlotThreadSafeInitializationTest.cpp
        tests/slot/ExtremeStressTest.cpp
)

target_include_directories(${PROJECT_TARGET_PREFIX}_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(
        ${PROJECT_TARGET_PREFIX}_tests PRIVATE
        ${PROJECT_TARGET_PREFIX}
        GTest::gtest
        GTest::gtest_main
)

# ---------------------------------------------------------------
# Benchmark Executable
# ---------------------------------------------------------------
add_executable(
        ${PROJECT_TARGET_PREFIX}_bench
        bench/slot/SlotThreadSafeBenchmark.cpp
)

target_include_directories(${PROJECT_TARGET_PREFIX}_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

target_link_libraries(${PROJECT_TARGET_PREFIX}_bench PRIVATE
        ${PROJECT_TARGET_PREFIX}
        benchmark::benchmark
        benchmark::benchmark_main
)

# ---------------------------------------------------------------
# Installation
# Installs the static library and public headers so the project
# can be consumed by downstream CMake projects via find_package.
# ---------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS ${PROJECT_TARGET_PREFIX}
        EXPORT ${PROJECT_TARGET_PREFIX}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_TARGET_PREFIX}
)