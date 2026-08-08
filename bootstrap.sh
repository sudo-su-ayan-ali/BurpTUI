#!/usr/bin/env bash
# bootstrap.sh — one-shot setup for BurpTUI on Debian/Ubuntu
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VCPKG_DIR="${REPO_DIR}/.vcpkg"
BUILD_DIR="${REPO_DIR}/build"

# ── 1. Install system build tools ────────────────────────
echo "[1/5] Installing system dependencies…"
sudo apt-get update -qq
sudo apt-get install -y \
    cmake ninja-build pkg-config \
    git curl zip unzip tar \
    build-essential libssl-dev

# ── 2. Bootstrap vcpkg (local clone) ─────────────────────
echo "[2/5] Bootstrapping vcpkg…"
if [[ ! -f "${VCPKG_DIR}/vcpkg" ]]; then
    git clone --depth=1 https://github.com/microsoft/vcpkg.git "${VCPKG_DIR}"
    "${VCPKG_DIR}/bootstrap-vcpkg.sh" -disableMetrics
fi
export VCPKG_ROOT="${VCPKG_DIR}"

# ── 3. CMake configure (vcpkg installs deps automatically) ──
echo "[3/5] Configuring CMake…"
cmake -S "${REPO_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_MANIFEST_DIR="${REPO_DIR}"

# ── 4. Compile ───────────────────────────────────────────
echo "[4/5] Building…"
cmake --build "${BUILD_DIR}" --parallel

# ── 5. Run tests ─────────────────────────────────────────
echo "[5/5] Running tests…"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo ""
echo "✅  Build succeeded!  Binary: ${BUILD_DIR}/burptui"
