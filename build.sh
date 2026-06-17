#!/bin/bash
# ============================================================================
# Project - OCI-Native Build Logic
# ============================================================================

set -e

# ============================================================================
# 1. HELPER FUNCTIONS & LOGGING
# ============================================================================

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log_step() { 
    echo -e "\n${MAGENTA}${BOLD}▶ PHASE: $PHASE | PROJECT: $PROJECT_NAME${NC}"
    echo -e "${MAGENTA}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }

# Method to run tests
run_tests() {
    "$TEST_BIN" --gtest_output="xml:$ROOT_DIR/test_results.xml"
    local test_exit_code=$?
    if [ $test_exit_code -ne 0 ]; then
        echo -e "${RED}[!] Tests failed with exit code: $test_exit_code${NC}"
    else
        echo -e "${GREEN}[PASS] Tests completed successfully.${NC}"
    fi
    return $test_exit_code
}

# Method to run benchmarks
run_benchmarks() {
    "$BENCH_BIN" --benchmark_out="$ROOT_DIR/bench_results.json" --benchmark_out_format=json
    local test_exit_code=$?
    if [ $test_exit_code -ne 0 ]; then
        echo -e "${RED}[!] Benchmarks failed with exit code: $test_exit_code${NC}"
    else
        echo -e "${GREEN}[PASS] Benchmarks completed successfully.${NC}"
    fi
    return $test_exit_code
}

# ============================================================================
# 2. ENVIRONMENT & ARGUMENT PARSING
# ============================================================================

# Load Project Environment Profiles
if [ -f .env ]; then
    export $(grep -v '^#' .env | xargs)
fi

# Fallback identity if variables are missing
PROJECT_NAME=${PROJECT_NAME:-"systic"}
DEFAULT_PHASE=${DEFAULT_PHASE:-"Release"}
CONAN_PROFILE=${CONAN_PROFILE:-"clang21"}

# Conan Server Registry Configuration & Fallbacks
CONAN_REMOTE_NAME=${CONAN_REMOTE_NAME:-"laptop_server"}
CONAN_REMOTE_URL=${CONAN_REMOTE_URL:-"http://172.17.0.1:9300"}
CONAN_LOGIN_USER=${CONAN_LOGIN_USER:-"systic_user"}
CONAN_LOGIN_PASSWORD=${CONAN_LOGIN_PASSWORD:-"sovereign_pass"}
CONAN_PACKAGE_USER=${CONAN_PACKAGE_USER:-"systic"}
CONAN_PACKAGE_CHANNEL=${CONAN_PACKAGE_CHANNEL:-"stable"}

# Parse Selection Phase
PHASE=$(echo "${1:-$DEFAULT_PHASE}" | awk '{print toupper(substr($0,1,1))tolower(substr($0,2))}')

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
        OUT_DIR=".build-dev"
        CMAKE_MODE="Debug"
        EXTRA_FLAGS="-g -O0 -DSYSTIC_DEV_MODE"
        ;;
    "Fix")
        OUT_DIR=".build-dev"
        CMAKE_MODE="Debug"
        EXTRA_FLAGS="-g -O0 -DSYSTIC_DEV_MODE"
        ;;
    "Publish")
        OUT_DIR=".build"
        CMAKE_MODE="Release"
        EXTRA_FLAGS="-O3 -DNDEBUG"
        ;;
    *)
        echo -e "${RED}❌ Unknown phase: $PHASE. Use Debug, Dev, Testing, Fix, Release, or Publish.${NC}"
        exit 1
        ;;
esac

log_step "Initializing Build Environment"

# ============================================================================
# 3. COMPILER & TOOLCHAIN DISCOVERY
# ============================================================================

if [ -z "$CC" ] || [ -z "$CXX" ]; then
    RAW_CXX=$(command -v clang++-21 || command -v clang++-19 || command -v clang++)
    if [ -n "$RAW_CXX" ]; then
        export CXX=$(realpath "$RAW_CXX")
        export CC=$(realpath $(echo "$CXX" | sed 's/++//'))
        log_info "Toolchain Resolved: $CXX"
    else
        echo -e "${RED}[ERROR] No Clang compiler found.${NC}"; exit 1
    fi
fi

# Clean CMake Cache if the compiler changed
if [ -f "$OUT_DIR/CMakeCache.txt" ]; then
    CACHED_CXX=$(grep "CMAKE_CXX_COMPILER:FILEPATH" "$OUT_DIR/CMakeCache.txt" | cut -d'=' -f2 || true)
    if [[ "$CACHED_CXX" != "$CXX" ]]; then
        log_info "Compiler mismatch in cache. Cleaning [$OUT_DIR]..."
        rm -rf "$OUT_DIR/CMakeCache.txt" "$OUT_DIR/CMakeFiles"
    fi
fi

# Ensure Conan 2.x directories exist
CONAN_HOME="${HOME}/.conan2"
mkdir -p "$CONAN_HOME"

if [ ! -f "$CONAN_HOME/global.conf" ]; then
    echo "tools.cmake.cmaketoolchain:generator=Ninja" > "$CONAN_HOME/global.conf"
    echo "tools.cmake:cmake_program=$(command -v cmake)" >> "$CONAN_HOME/global.conf"
fi

# ============================================================================
# 4. GLOBAL CONAN REMOTE & AUTH SETUP (Fixed Order of Operations)
# ============================================================================
log_info "Configuring remote registry mapping..."
conan remote add "${CONAN_REMOTE_NAME}" "${CONAN_REMOTE_URL}" --force

log_info "Authenticating to remote storage context..."
conan remote login "${CONAN_REMOTE_NAME}" "${CONAN_LOGIN_USER}" --password "${CONAN_LOGIN_PASSWORD}"

# ============================================================================
# 5. CORE COMPILATION STEPS (Except for Fix phase)
# ============================================================================
if [[ "$PHASE" != "Fix" ]]; then
    mkdir -p "$OUT_DIR"
    
    # Conan Install Dependencies
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

    # CMake Configure
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

    # Native System Compile execution
    log_info "Step 4: Compiling"
    cmake --build "$OUT_DIR" -j$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN)
fi

# ============================================================================
# 6. PHASE EXECUTION DRIVERS
# ============================================================================

# --- PHASE: DEV & TESTING (Execution & Verification) ---
if [[ "$PHASE" == "Testing" || "$PHASE" == "Dev" ]]; then
    log_step "Step 5: Execution & Artifact"
    ROOT_DIR=$(pwd)

    TEST_BIN=$(find "$OUT_DIR" -name "*tests" -type f -executable | head -n 1)
    BENCH_BIN=$(find "$OUT_DIR" -name "*bench" -type f -executable | head -n 1)

    if [ -n "$TEST_BIN" ]; then
        log_info "Running Tests: $TEST_BIN"
        run_tests
        PIPELINE_TEST_STATUS=$?
    fi

    if [ -n "$BENCH_BIN" ]; then
        log_info "Running Benchmarks: $BENCH_BIN"
        run_benchmarks
        PIPELINE_BENCH_STATUS=$?
    fi

    echo -e "\n${CYAN}--- Artifact Handshake ---${NC}"
    if [ -n "$PIPELINE_TEST_STATUS" ]; then
          [ -f "$ROOT_DIR/test_results.xml" ] && echo -e "  ${GREEN}[PASS]${NC} test_results.xml -> Host" || echo -e "  ${RED}[FAIL]${NC} No test report found."
    fi
    if [ -n "$PIPELINE_BENCH_STATUS" ]; then
          [ -f "$ROOT_DIR/bench_results.json" ] && echo -e "  ${GREEN}[PASS]${NC} bench_results.json -> Host" || echo -e "  ${RED}[FAIL]${NC} No bench report found."
    fi
fi

# --- PHASE: FIX (Clang-Tidy Auto-remediation) ---
if [[ "$PHASE" == "Fix" ]]; then
    COMPILE_DB="$OUT_DIR/compile_commands.json"

    if [ ! -f "$COMPILE_DB" ]; then
        echo -e "${RED}[ERROR] No compile_commands.json found at $COMPILE_DB${NC}"
        echo -e "${BLUE}[INFO]${NC} Run './build.sh Dev' first to generate it."
        exit 1
    fi

    TIDY_EXE="clang-tidy"
    log_info "clang-tidy fix — using: $TIDY_EXE"

    SOURCES=$(find src include tests bench -name "*.cpp" -o -name "*.hpp" 2>/dev/null | sort)
    if [ -z "$SOURCES" ]; then
        echo -e "${RED}[ERROR] No source files found under src/ include/ tests/ bench/${NC}"
        exit 1
    fi

    echo -e "${CYAN}--- Files to fix ---${NC}"
    echo "$SOURCES" | while read -r f; do echo "  $f"; done

    echo -e "\n${MAGENTA}Running clang-tidy --fix ...${NC}"
    "$TIDY_EXE" -p "$COMPILE_DB" --fix --fix-errors --format-style=file $SOURCES

    FIX_EXIT=$?
    if [ $FIX_EXIT -eq 0 ]; then
        echo -e "\n${GREEN}${BOLD}✅ clang-tidy --fix completed. Review changes with: git diff${NC}"
    else
        echo -e "\n${RED}[WARN] clang-tidy exited with code $FIX_EXIT — manual fixes needed.${NC}"
    fi
fi

# --- PHASE: PUBLISH (Package Local Creation & Upstream Upload) ---
if [[ "$PHASE" == "Publish" ]]; then
    log_step "Step 5: Publishing Release Package"

    # Extract recipe data
    if [ -f "conanfile.py" ]; then
        PKG_VERSION=$(grep -E "version\s*=\s*[\"']" conanfile.py | sed -E "s/version\s*=\s*[\"']([^\"']+)[\"']/\1/" | xargs)
        PKG_NAME=$(grep -E "name\s*=\s*[\"']" conanfile.py | sed -E "s/name\s*=\s*[\"']([^\"']+)[\"']/\1/" | xargs)
    fi

    PKG_NAME=${PKG_NAME:-"arrayslotthreadsafe"}
    PKG_VERSION=${PKG_VERSION:-"0.1.1"}
    FULL_PACKAGE_REF="${PKG_NAME}/${PKG_VERSION}@${CONAN_PACKAGE_USER}/${CONAN_PACKAGE_CHANNEL}"

    log_info "Target Reference Identified: ${FULL_PACKAGE_REF}"
    log_info "Enforcing local recipe cache packaging..."
    
    conan create . \
        --user="${CONAN_PACKAGE_USER}" \
        --channel="${CONAN_PACKAGE_CHANNEL}" \
        -pr:b="./.conan/profiles/$CONAN_PROFILE" \
        -pr:h="./.conan/profiles/$CONAN_PROFILE" \
        -s build_type="Release" --build=missing

    log_info "Uploading fully compiled packages & recipe upstream to ${CONAN_REMOTE_NAME}..."
    conan upload "${FULL_PACKAGE_REF}" --remote="${CONAN_REMOTE_NAME}" --confirm

    echo -e "\n${GREEN}${BOLD}🚀 Package successfully published to upstream server registry!${NC}"
fi

echo -e "\n${GREEN}${BOLD}✅ $PROJECT_NAME [$PHASE] READY!${NC}"