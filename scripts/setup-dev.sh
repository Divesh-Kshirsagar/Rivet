#!/usr/bin/env bash
# =============================================================================
# scripts/setup-dev.sh — Install build dependencies for Rivet compiler development
# =============================================================================
# This script installs everything needed to build the Rivet compiler from source:
#   - LLVM 18 (dev libraries + headers)
#   - Clang 18 (C++ compiler)
#   - CMake, Ninja (build system)
#   - ARM cross-compilation toolchain (gcc-arm-none-eabi, newlib)
#   - Renode (hardware emulator for ARM demos)
#
# Usage:
#   sudo bash scripts/setup-dev.sh
#
# NOTE: This is NOT the same as install.sh, which downloads prebuilt release
#       binaries. This script sets up a development environment for building
#       the compiler from source.
# =============================================================================
set -euo pipefail

RENODE_VERSION="${RENODE_VERSION:-1.16.1}"

# ---------------------------------------------------------------------------
# Require root
# ---------------------------------------------------------------------------
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root (or via sudo)." >&2
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive

echo "==> Installing Rivet build dependencies..."

# ---------------------------------------------------------------------------
# Core build toolchain + LLVM 18
# ---------------------------------------------------------------------------
apt-get update
apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    clang-18 \
    llvm-18-dev \
    lld-18 \
    libzstd-dev \
    libffi-dev \
    ca-certificates \
    wget

# ---------------------------------------------------------------------------
# ARM bare-metal cross-compilation toolchain
# ---------------------------------------------------------------------------
apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    binutils-arm-none-eabi

# ---------------------------------------------------------------------------
# Set clang-18 as default clang
# ---------------------------------------------------------------------------
update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-18   100 || true
update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 || true

# ---------------------------------------------------------------------------
# Install Renode (hardware emulator)
# ---------------------------------------------------------------------------
echo "==> Installing Renode v${RENODE_VERSION}..."
wget -q "https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode_${RENODE_VERSION}_amd64.deb" \
    -O /tmp/renode.deb
apt-get install -y /tmp/renode.deb || (apt-get install -y -f && apt-get install -y /tmp/renode.deb)
rm -f /tmp/renode.deb

# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------
apt-get clean
rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "============================================="
echo "  Rivet development environment ready"
echo "============================================="
echo "  LLVM:     $(llvm-config-18 --version 2>/dev/null || echo 'N/A')"
echo "  Clang:    $(clang-18 --version 2>/dev/null | head -1 || echo 'N/A')"
echo "  CMake:    $(cmake --version 2>/dev/null | head -1 || echo 'N/A')"
echo "  Renode:   $(renode --version 2>/dev/null | head -1 || echo 'N/A')"
echo "  ARM GCC:  $(arm-none-eabi-gcc --version 2>/dev/null | head -1 || echo 'N/A')"
echo "============================================="
echo ""
echo "Next steps:"
echo "  cmake -S . -B build -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18"
echo "  cmake --build build -j\$(nproc)"
echo "  ./build/rivet tests/validation.rvt --dump-ast"
