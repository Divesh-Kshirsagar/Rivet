#!/usr/bin/env bash
set -euo pipefail

REPO="${RIVET_REPO:-diveshkshirsagar/RIVET}"
VERSION="${RIVET_VERSION:-latest}"
INSTALL_DIR="${RIVET_INSTALL_DIR:-/usr/local/bin}"
DRY_RUN="${RIVET_DRY_RUN:-0}"

OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH_RAW="$(uname -m)"

case "$OS" in
  linux) PLATFORM="linux" ;;
  darwin) PLATFORM="macos" ;;
  *)
    echo "Unsupported OS: $OS" >&2
    exit 1
    ;;
esac

case "$ARCH_RAW" in
  x86_64|amd64) ARCH="x86_64" ;;
  aarch64|arm64) ARCH="arm64" ;;
  *)
    echo "Unsupported architecture: $ARCH_RAW" >&2
    exit 1
    ;;
esac

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is required." >&2
  exit 1
fi
if ! command -v tar >/dev/null 2>&1; then
  echo "tar is required." >&2
  exit 1
fi

if [[ "$VERSION" == "latest" ]]; then
  RELEASE_JSON="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest")"
  if command -v jq >/dev/null 2>&1; then
    TAG="$(printf '%s' "$RELEASE_JSON" | jq -r '.tag_name // empty')"
  else
    TAG="$(printf '%s' "$RELEASE_JSON" | sed -n 's/.*"tag_name": "\([^"]*\)".*/\1/p' | head -n1)"
  fi
  if [[ -z "$TAG" ]]; then
    echo "Failed to resolve latest release tag from ${REPO}." >&2
    exit 1
  fi
else
  TAG="$VERSION"
  if [[ "$TAG" != v* ]]; then
    TAG="v${TAG}"
  fi
fi

ARTIFACT="rivet-${TAG}-${PLATFORM}-${ARCH}.tar.gz"
URL="https://github.com/${REPO}/releases/download/${TAG}/${ARTIFACT}"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

ARCHIVE_PATH="${TMP_DIR}/${ARTIFACT}"

echo "Downloading ${URL}"
curl -fL "$URL" -o "$ARCHIVE_PATH"

tar -xzf "$ARCHIVE_PATH" -C "$TMP_DIR"
BIN_PATH="$(find "$TMP_DIR" -type f -path '*/bin/rivet' | head -n1)"

if [[ -z "$BIN_PATH" || ! -f "$BIN_PATH" ]]; then
  echo "Could not find rivet binary in downloaded archive." >&2
  exit 1
fi

INSTALL_PATH="$INSTALL_DIR/rivet"

can_write_install_dir() {
  [[ -d "$INSTALL_DIR" && -w "$INSTALL_DIR" ]] && return 0
  [[ ! -e "$INSTALL_DIR" && -w "$(dirname "$INSTALL_DIR")" ]] && return 0
  return 1
}

if can_write_install_dir; then
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[dry-run] Would install rivet to $INSTALL_PATH"
    exit 0
  fi
  mkdir -p "$INSTALL_DIR"
  cp "$BIN_PATH" "$INSTALL_PATH"
  chmod +x "$INSTALL_PATH"
else
  if command -v sudo >/dev/null 2>&1; then
    if [[ "$DRY_RUN" == "1" ]]; then
      echo "[dry-run] Would install rivet to $INSTALL_PATH using sudo"
      exit 0
    fi
    echo "Installing to $INSTALL_DIR requires elevated permissions. Requesting sudo..."
    sudo mkdir -p "$INSTALL_DIR"
    sudo cp "$BIN_PATH" "$INSTALL_PATH"
    sudo chmod +x "$INSTALL_PATH"
  else
    FALLBACK_DIR="$HOME/.local/bin"
    echo "Cannot write to $INSTALL_DIR and sudo is not available."
    echo "Falling back to $FALLBACK_DIR."
    if [[ "$DRY_RUN" == "1" ]]; then
      echo "[dry-run] Would install rivet to $FALLBACK_DIR/rivet"
      exit 0
    fi
    mkdir -p "$FALLBACK_DIR"
    INSTALL_DIR="$FALLBACK_DIR"
    INSTALL_PATH="$INSTALL_DIR/rivet"
    cp "$BIN_PATH" "$INSTALL_PATH"
    chmod +x "$INSTALL_PATH"
  fi
fi

if [[ ! -x "$INSTALL_PATH" ]]; then
  echo "Install failed: rivet binary is not executable at $INSTALL_PATH" >&2
  exit 1
fi

echo "Installed rivet to $INSTALL_PATH"
case ":$PATH:" in
  *":$INSTALL_DIR:"*)
    echo "rivet is available on PATH."
    ;;
  *)
    echo "Add this to your shell profile:"
    echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
    ;;
esac
