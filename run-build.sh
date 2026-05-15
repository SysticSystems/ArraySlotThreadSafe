#!/bin/bash
# ============================================================================
# Project Systic - OCI Build Wrapper (Strict Naming)
# ============================================================================

set -e

# --- Load Environment Variables ---
if [ -f .env ]; then
    export $(grep -v '^#' .env | xargs)
else
    echo -e "\033[0;31m[ERROR] .env file not found!\033[0m"
    exit 1
fi

# --- Strict Naming Enforcement ---
PROJECT_NAME=${PROJECT_NAME:-"systic"}
IMAGE_NAME="${PROJECT_NAME}_build_image"
CONTAINER_NAME="${PROJECT_NAME}_build_container"
PHASE=$(echo "${1:-$DEFAULT_PHASE}" | awk '{print toupper(substr($0,1,1))tolower(substr($0,2))}')

# Color definitions
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }

# 1. Image Lifecycle
if [[ "$(docker images -q $IMAGE_NAME 2> /dev/null)" == "" ]]; then
    log_info "Image '$IMAGE_NAME' not found. Building..."
    docker build -t "$IMAGE_NAME" ./.github/workflows/
else
    log_info "Using Build Image: $IMAGE_NAME"
fi

# 2. Container Lifecycle (Force Cleanup)
if [ "$(docker ps -aq -f name=^/${CONTAINER_NAME}$)" ]; then
    log_info "Cleaning up stale container: $CONTAINER_NAME"
    docker rm -f "$CONTAINER_NAME" > /dev/null
fi

log_info "Launching Sovereign Build: $PROJECT_NAME [$PHASE]"

# 3. Execution Handshake
# Maps host to /workspace and ensures correct file ownership on host
docker run --name "$CONTAINER_NAME" \
    -v "$(pwd):/workspace" \
    -w /workspace \
    "$IMAGE_NAME" \
    ./build.sh "$PHASE"

echo -e "${CYAN}${BOLD}✅ Build Complete. Output in .build-${PHASE,,}${NC}"