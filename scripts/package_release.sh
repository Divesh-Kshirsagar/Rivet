#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-0.0.1}"
TAG="v${VERSION}"
BUILD_DIR="build-release"
DIST_DIR="dist"

OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH_RAW="$(uname -m)"
case "$ARCH_RAW" in
  x86_64|amd64) ARCH="x86_64" ;;
  aarch64|arm64) ARCH="arm64" ;;
  *)
    echo "Unsupported architecture: $ARCH_RAW" >&2
    exit 1
    ;;
esac

case "$OS" in
  linux) PLATFORM="linux" ;;
  darwin) PLATFORM="macos" ;;
  *)
    echo "Unsupported OS: $OS" >&2
    exit 1
    ;;
esac

ARTIFACT_BASE="rivet-${TAG}-${PLATFORM}-${ARCH}"
STAGE_DIR="${DIST_DIR}/${ARTIFACT_BASE}"
ARCHIVE_PATH="${DIST_DIR}/${ARTIFACT_BASE}.tar.gz"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/bin"
cp "$BUILD_DIR/rivet" "$STAGE_DIR/bin/rivet"
cp README.md CHANGELOG.md LICENSE "$STAGE_DIR/"

rm -f "$ARCHIVE_PATH"
tar -C "$DIST_DIR" -czf "$ARCHIVE_PATH" "$ARTIFACT_BASE"

echo "Created release artifact: $ARCHIVE_PATH"
