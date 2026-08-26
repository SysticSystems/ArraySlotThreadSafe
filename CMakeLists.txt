cmake_minimum_required(VERSION 3.28)
project(arrayslotthreadsafe VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

option(BUILD_TESTING "Build the test suite" OFF)
option(BUILD_BENCHMARKING "Build the benchmarks" OFF)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "This project requires Clang. Found: ${CMAKE_CXX_COMPILER_ID}")
endif ()

if (NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "C++ module dependency scanning requires the Ninja generator. Use -G Ninja.")
endif ()

# ---------------------------------------------------------------
# Core Modular Library
# ---------------------------------------------------------------
add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS src
    FILES
        src/CpuIntrinsics.ixx
        src/SlotOperationResult.ixx
        src/SlotThreadSafe.ixx
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

# ---------------------------------------------------------------
# EMBED SOURCE TEXT DIRECTLY INTO THE .PCM ARTIFACTS
# This makes the BMIs totally self-contained and path-independent.
# ---------------------------------------------------------------
target_compile_options(${PROJECT_NAME} PUBLIC 
    "-fmodules-embed-all-files"
)

# ---------------------------------------------------------------
# Test & Benchmark Executables
# ---------------------------------------------------------------
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(${PROJECT_NAME}_tests
            tests/slot/SlotThreadSafeInitializationTest.cpp
            tests/slot/SlotThreadSafeStressTest.cpp
    )
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME} GTest::gtest GTest::gtest_main)
endif()

if(BUILD_BENCHMARKING)
    find_package(benchmark REQUIRED)
    add_executable(${PROJECT_NAME}_bench bench/slot/SlotThreadSafeBenchmark.cpp)
    target_link_libraries(${PROJECT_NAME}_bench PRIVATE ${PROJECT_NAME} benchmark::benchmark benchmark::benchmark_main)
endif()

# ---------------------------------------------------------------
# Installation
# ---------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cxx-modules
)

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    CXX_MODULES_DIRECTORY cxx-modules
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
"include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Targets.cmake\")\n"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)