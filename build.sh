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

# Conan Server Registry Fallbacks
CONAN_REMOTE_NAME=${CONAN_REMOTE_NAME:-"laptop_server"}
CONAN_REMOTE_URL=${CONAN_REMOTE_URL:-"http://172.17.0.1:9300"}
CONAN_LOGIN_USER=${CONAN_LOGIN_USER:-"systic_user"}
CONAN_LOGIN_PASSWORD=${CONAN_LOGIN_PASSWORD:-"sovereign_pass"}
CONAN_PACKAGE_USER=${CONAN_PACKAGE_USER:-"systic"}
CONAN_PACKAGE_CHANNEL=${CONAN_PACKAGE_CHANNEL:-"stable"}

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
    "Fix")
        # Auto-fix clang-tidy violations in-place.
        # Requires compile_commands.json — run Dev first if missing.
        OUT_DIR=".build-dev"
        CMAKE_MODE="Debug"
        EXTRA_FLAGS="-g -O0 -DSYSTIC_DEV_MODE"
        ;;
    "Publish")
        # Registry Deployment Phase
        OUT_DIR=".build"
        CMAKE_MODE="Release"
        EXTRA_FLAGS="-O3 -DNDEBUG"
        ;;

    *)
        echo -e "${RED}❌ Unknown phase: $PHASE. Use Debug, Dev, Testing, Fix, Release, or Publish.${NC}"
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
# Method to run test
run_tests() {
    # 1. Run the binary cleanly
    "$TEST_BIN" --gtest_output="xml:$ROOT_DIR/test_results.xml"

    # 2. CAPTURE IMMEDIATELY (Crucial: $? changes after every single command)
    local test_exit_code=$?

    # 3. Use the return value multiple times safely
    if [ $test_exit_code -ne 0 ]; then
        echo -e "${RED}[!] Tests failed with exit code: $test_exit_code${NC}"
    else
        echo -e "${GREEN}[PASS] Tests completed successfully.${NC}"
    fi

    # 4. Return it from the function so the outer script can capture it too
    return $test_exit_code
}

# Method that executes the benchmarks tests.
run_benchmarks() {
  # 1. Run the binary cleanly
      "$BENCH_BIN" --benchmark_out="$ROOT_DIR/bench_results.json" --benchmark_out_format=json

      # 2. CAPTURE IMMEDIATELY (Crucial: $? changes after every single command)
      local test_exit_code=$?

      # 3. Use the return value multiple times safely
      if [ $test_exit_code -ne 0 ]; then
          echo -e "${RED}[!] Benchmarks failed with exit code: $test_exit_code${NC}"
      else
          echo -e "${GREEN}[PASS] Benchmarks completed successfully.${NC}"
      fi

      # 4. Return it from the function so the outer script can capture it too
      return $test_exit_code
}

if [[ "$PHASE" == "Testing" || "$PHASE" == "Dev" ]]; then
    log_step "Step 5: Execution & Artifact"

    # Define artifact paths relative to workspace root
    ROOT_DIR=$(pwd)

    # Search for specific binaries
    TEST_BIN=$(find "$OUT_DIR" -name "*tests" -type f -executable | head -n 1)
    BENCH_BIN=$(find "$OUT_DIR" -name "*bench" -type f -executable | head -n 1)

    if [ -n "$TEST_BIN" ]; then
        log_info "Running Tests: $TEST_BIN"
        run_tests
        PIPELINE_TEST_STATUS=$?
    fi

    if [ -n "$BENCH_BIN" ]; then
        log_info "Running Benchmarks: $BENCH_BIN"
        # Outputting directly to the mapped host volume
        run_benchmarks
        PIPELINE_BENCH_STATUS=$?
    fi

    # Verification log for the developer
    echo -e "\n${CYAN}--- Artifact Handshake ---${NC}"
    if [ -n "$PIPELINE_TEST_STATUS" ]; then
          [ -f "$ROOT_DIR/test_results.xml" ] && echo -e "  ${GREEN}[PASS]${NC} test_results.xml -> Host" || echo -e "  ${RED}[FAIL]${NC} No test report found."
    fi

    if [ -n "$PIPELINE_BENCH_STATUS" ]; then
      [ -f "$ROOT_DIR/bench_results.json" ] && echo -e "  ${GREEN}[PASS]${NC} bench_results.json -> Host" || echo -e "  ${RED}[FAIL]${NC} No bench report found."
    fi

fi
# ---------------------------------------------------------------
# Fix Phase — clang-tidy auto-remediation
# Requires compile_commands.json produced by a prior Dev build.
# Run: ./build.sh Fix
# ---------------------------------------------------------------
if [[ "$PHASE" == "Fix" ]]; then
    COMPILE_DB="$OUT_DIR/compile_commands.json"

    if [ ! -f "$COMPILE_DB" ]; then
        echo -e "${RED}[ERROR] No compile_commands.json found at $COMPILE_DB${NC}"
        echo -e "${BLUE}[INFO]${NC} Run './build.sh Dev' first to generate it."
        exit 1
    fi

    # Discover clang-tidy (same priority order as CMake)
    TIDY_EXE="clang-tidy"
    if [ -z "$TIDY_EXE" ]; then
        echo -e "${RED}[ERROR] clang-tidy not found in PATH.${NC}"; exit 1
    fi

    log_info "clang-tidy fix — using: $TIDY_EXE"
    log_info "Compile DB : $COMPILE_DB"
    log_info "Config     : .clang-tidy (project root)"

    # Collect all project source files (exclude build dirs and deps)
    SOURCES=$(find src include tests bench \
        -name "*.cpp" -o -name "*.hpp" 2>/dev/null | sort)

    if [ -z "$SOURCES" ]; then
        echo -e "${RED}[ERROR] No source files found under src/ include/ tests/ bench/${NC}"
        exit 1
    fi

    echo -e "${CYAN}--- Files to fix ---${NC}"
    echo "$SOURCES" | while read -r f; do echo "  $f"; done

    echo -e "\n${MAGENTA}Running clang-tidy --fix ...${NC}"
    # shellcheck disable=SC2086
    "$TIDY_EXE" \
        -p "$COMPILE_DB" \
        --fix \
        --fix-errors \
        --format-style=file \
        $SOURCES

    FIX_EXIT=$?
    if [ $FIX_EXIT -eq 0 ]; then
        echo -e "\n${GREEN}${BOLD}✅ clang-tidy --fix completed. Review changes with: git diff${NC}"
    else
        echo -e "\n${RED}[WARN] clang-tidy exited with code $FIX_EXIT — some violations need manual fixes.${NC}"
        echo -e "${BLUE}[INFO]${NC} Check remaining issues with: ./build.sh Dev"
    fi
fi

# ---------------------------------------------------------------
# Publish Phase — Registry Deployment Logic
# Run: ./build.sh Publish
# ---------------------------------------------------------------
if [[ "$PHASE" == "Publish" ]]; then
    log_step "Step 5: Publishing Release Package"

    # 1. Parse project version dynamically out of conanfile.py
    if [ -f "conanfile.py" ]; then
        PKG_VERSION=$(grep -E "version\s*=\s*[\"']" conanfile.py | sed -E "s/version\s*=\s*[\"']([^\"']+)[\"']/\1/" | xargs)
        PKG_NAME=$(grep -E "name\s*=\s*[\"']" conanfile.py | sed -E "s/name\s*=\s*[\"']([^\"']+)[\"']/\1/" | xargs)
    fi

    # Fallbacks if regex extraction hits structural issues
    PKG_NAME=${PKG_NAME:-"arrayslotthreadsafe"}
    PKG_VERSION=${PKG_VERSION:-"0.1.1"}

    log_info "Target Reference Identified: ${PKG_NAME}/${PKG_VERSION}@${CONAN_PACKAGE_USER}/${CONAN_PACKAGE_CHANNEL}"
    log_info "Remote Server Registry End: ${CONAN_REMOTE_URL}"

    # 2. Local Conan Create Execution to ensure the recipe/binary artifact is in cache
    log_info "Enforcing local recipe cache packaging..."
    conan create . \
        --user="${CONAN_PACKAGE_USER}" \
        --channel="${CONAN_PACKAGE_CHANNEL}" \
        -pr:b="./.conan/profiles/$CONAN_PROFILE" \
        -pr:h="./.conan/profiles/$CONAN_PROFILE" \
        -s build_type="Release" --build=missing

    # 3. Dynamic Remote Registration
    log_info "Configuring remote registry mapping..."
    conan remote add "${CONAN_REMOTE_NAME}" "${CONAN_REMOTE_URL}" --force

    # 4. Authentication Check
    log_info "Authenticating to remote storage..."
    conan remote login "${CONAN_REMOTE_NAME}" "${CONAN_LOGIN_USER}" -p "${CONAN_LOGIN_PASSWORD}"

    # 5. Build Package Reference String and Push Upstream
    FULL_PACKAGE_REF="${PKG_NAME}/${PKG_VERSION}@${CONAN_PACKAGE_USER}/${CONAN_PACKAGE_CHANNEL}"

    log_info "Uploading fully compiled packages & recipe to server..."
    conan upload "${FULL_PACKAGE_REF}" --remote="${CONAN_REMOTE_NAME}" --confirm

    echo -e "\n${GREEN}${BOLD}🚀 Package successfully published to upstream server registry!${NC}"
fi

echo -e "\n${GREEN}${BOLD}✅ $PROJECT_NAME [$PHASE] READY!${NC}"