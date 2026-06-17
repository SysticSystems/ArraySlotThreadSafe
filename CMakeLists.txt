cmake_minimum_required(VERSION 3.28) # Bumped to 3.28+ for robust C++23 Module installations
project(ArraySlotThreadSafe VERSION 0.4.0 LANGUAGES CXX)

set(PROJECT_TARGET_PREFIX ${PROJECT_NAME})

# ---------------------------------------------------------------
# Language Standard & Module Scanning
# ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# ---------------------------------------------------------------
# Compiler Guard
# ---------------------------------------------------------------
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
            "This project requires Clang. Found: ${CMAKE_CXX_COMPILER_ID}. "
            "Update your Conan profile or set CMAKE_CXX_COMPILER explicitly."
    )
endif ()

# ---------------------------------------------------------------
# Dependencies (Conan / CMakeDeps)
# ---------------------------------------------------------------
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# ---------------------------------------------------------------
# Core Modular Library
# ---------------------------------------------------------------
add_library(${PROJECT_TARGET_PREFIX})

target_sources(${PROJECT_TARGET_PREFIX}
    PUBLIC
    FILE_SET CXX_MODULES FILES
        src/CpuIntrinsics.ixx           # Module Partition (:CpuIntrinsics)
        src/ArraySlotThreadSafe.ixx     # Primary Module Interface Gateway
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

target_link_libraries(${PROJECT_TARGET_PREFIX}_tests PRIVATE
        ${PROJECT_TARGET_PREFIX}        # Imports the module automatically!
        GTest::gtest
        GTest::gtest_main
)

# ---------------------------------------------------------------
# Benchmark Executable
# ---------------------------------------------------------------
add_executable(${PROJECT_TARGET_PREFIX}_bench
        bench/slot/SlotThreadSafeBenchmark.cpp
)

target_include_directories(${PROJECT_TARGET_PREFIX}_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

target_link_libraries(${PROJECT_TARGET_PREFIX}_bench PRIVATE
        ${PROJECT_TARGET_PREFIX}        # Imports the module automatically!
        benchmark::benchmark
        benchmark::benchmark_main
)

# ---------------------------------------------------------------
# Installation (Purely Modular)
# ---------------------------------------------------------------
include(GNUInstallDirs)

# Install the static binary and the generated Binary Module Interfaces (.pcm paths)
install(TARGETS ${PROJECT_TARGET_PREFIX}
        EXPORT ${PROJECT_TARGET_PREFIX}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_TARGET_PREFIX}
)

# Physically write the export metadata targets config to disk
install(EXPORT ${PROJECT_TARGET_PREFIX}Targets
        FILE ${PROJECT_TARGET_PREFIX}Targets.cmake
        NAMESPACE ${PROJECT_TARGET_PREFIX}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_TARGET_PREFIX}
)