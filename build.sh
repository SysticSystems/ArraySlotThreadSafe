#!/bin/bash
# ============================================================================
# Project - OCI-Native Build Logic
# ============================================================================

set -e

# --- Load Project Environment ---
if [ -f .env ]; then
    export $(grep -v '^#' .env | xargs)
fi

# Fallback identity if .env is missing
PROJECT_NAME=${PROJECT_NAME:-"systic"}
DEFAULT_PHASE=${DEFAULT_PHASE:-"Release"}
CONAN_PROFILE=${CONAN_PROFILE:-"clang21"}

# 1. Parse Phase
PHASE=$(echo "${1:-$DEFAULT_PHASE}" | awk '{print toupper(substr($0,1,1))tolower(substr($0,2))}')

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

case "$PHASE" in
    "Debug")
        OUT_DIR=".build-debug"
        CMAKE_MODE="Debug"
        EXTRA_FLAGS="-g -O0"
        ;;
    "Testing")
        OUT_DIR=".build-test"
        CMAKE_MODE="RelWithDebInfo"
        EXTRA_FLAGS="-O3 -g -DSYSTIC_FULL_ASSERT"
        ;;
    "Release")
        OUT_DIR=".build"
        CMAKE_MODE="Release"
        EXTRA_FLAGS="-O3 -fomit-frame-pointer -DNDEBUG"
        ;;
    "Dev")
        # Fast Dev path with basic assertions
        OUT_DIR=".build-dev"
        CMAKE_MODE="Debug"
        EXTRA_FLAGS="-g -O0 -DSYSTIC_DEV_MODE"
        ;;
    *)
        echo -e "${RED}❌ Unknown phase: $PHASE. Use Debug, Dev, Testing, or Release.${NC}"
        exit 1
        ;;
esac

log_step() { echo -e "\n${MAGENTA}${BOLD}▶ PHASE: $PHASE | PROJECT: $PROJECT_NAME${NC}\n${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"; }
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }

log_step "Initializing Build Environment"

# --- COMPILER DISCOVERY ---
if [ -z "$CC" ] || [ -z "$CXX" ]; then
    # Always resolve to absolute paths to prevent CMake "not found" errors
    RAW_CXX=$(command -v clang++-21 || command -v clang++-19 || command -v clang++)
    if [ -n "$RAW_CXX" ]; then
        export CXX=$(realpath "$RAW_CXX")
        export CC=$(realpath $(echo "$CXX" | sed 's/++//'))
        log_info "Toolchain Resolved: $CXX"
    else
        echo -e "${RED}[ERROR] No Clang compiler found.${NC}"; exit 1
    fi
fi

# --- CLEANUP ---
if [ -f "$OUT_DIR/CMakeCache.txt" ]; then
    # Wipe cache if it points to a different compiler or phase
    CACHED_CXX=$(grep "CMAKE_CXX_COMPILER:FILEPATH" "$OUT_DIR/CMakeCache.txt" | cut -d'=' -f2 || true)
    if [[ "$CACHED_CXX" != "$CXX" ]]; then
        log_info "Compiler mismatch in cache. Cleaning [$OUT_DIR]..."
        rm -rf "$OUT_DIR/CMakeCache.txt" "$OUT_DIR/CMakeFiles"
    fi
fi

# Ensure Conan 2.x environment is stable
CONAN_HOME="${HOME}/.conan2"
mkdir -p "$CONAN_HOME"

if [ ! -f "$CONAN_HOME/global.conf" ]; then
    echo "tools.cmake.cmaketoolchain:generator=Ninja" > "$CONAN_HOME/global.conf"
    echo "tools.cmake:cmake_program=$(command -v cmake)" >> "$CONAN_HOME/global.conf"
fi

# --- CONAN INSTALL ---
mkdir -p "$OUT_DIR"
if [ ! -f "$OUT_DIR/conan_toolchain.cmake" ]; then
    log_info "Step 2: Conan install (Profile: $CONAN_PROFILE)"
    conan install . -of "$OUT_DIR" --build=missing \
         -pr:b="./.conan/profiles/$CONAN_PROFILE" \
         -pr:h="./.conan/profiles/$CONAN_PROFILE" \
         -s build_type="$CMAKE_MODE" \
         -g CMakeDeps -g CMakeToolchain
else
    log_info "Step 2: Skipping Conan (Cache hit)"
fi

# --- CMAKE CONFIGURE & BUILD ---
TOOLCHAIN_FILE=$(find "$OUT_DIR" -name "conan_toolchain.cmake" | head -n 1)
[ -f "$OUT_DIR/conanbuild.sh" ] && source "$OUT_DIR/conanbuild.sh"

log_info "Step 3: CMake Configure (Ninja)"
cmake -G "Ninja" -S . -B "$OUT_DIR" \
     -DCMAKE_TOOLCHAIN_FILE="$(realpath "$TOOLCHAIN_FILE")" \
     -DCMAKE_BUILD_TYPE="$CMAKE_MODE" \
     -DCMAKE_CXX_COMPILER="$CXX" \
     -DCMAKE_C_COMPILER="$CC" \
     -DCMAKE_CXX_FLAGS="$EXTRA_FLAGS" \
     -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

log_info "Step 4: Compiling"
cmake --build "$OUT_DIR" -j$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN)

# --- EXECUTION & ARTIFACTS ---
if [[ "$PHASE" == "Testing" || "$PHASE" == "Dev" ]]; then
    log_step "Step 5: Execution & Artifact"

    # Define artifact paths relative to workspace root
    ROOT_DIR=$(pwd)
    
    # Search for specific binaries
    TEST_BIN=$(find "$OUT_DIR" -name "*tests" -type f -executable | head -n 1)
    BENCH_BIN=$(find "$OUT_DIR" -name "*bench" -type f -executable | head -n 1)

    if [ -n "$TEST_BIN" ]; then
        log_info "Running Tests: $TEST_BIN"
        # Outputting directly to the mapped host volume
        "$TEST_BIN" --gtest_output="xml:$ROOT_DIR/test_results.xml" || echo -e "${RED}[!] Tests failed${NC}"
    fi

    if [ -n "$BENCH_BIN" ]; then
        log_info "Running Benchmarks: $BENCH_BIN"
        # Outputting directly to the mapped host volume
        "$BENCH_BIN" --benchmark_out="$ROOT_DIR/bench_results.json" --benchmark_out_format=json || echo -e "${RED}[!] Benchmarks failed${NC}"
    fi

    # Verification log for the developer
    echo -e "\n${CYAN}--- Artifact Handshake ---${NC}"
    [ -f "$ROOT_DIR/test_results.xml" ] && echo -e "  ${GREEN}[PASS]${NC} test_results.xml -> Host" || echo -e "  ${RED}[FAIL]${NC} No test report found."
    [ -f "$ROOT_DIR/bench_results.json" ] && echo -e "  ${GREEN}[PASS]${NC} bench_results.json -> Host" || echo -e "  ${RED}[FAIL]${NC} No bench report found."
fi
echo -e "\n${GREEN}${BOLD}✅ $PROJECT_NAME [$PHASE] READY!${NC}"