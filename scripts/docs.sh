#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p docs/doxygen/_build docs/site

# 1. Build Doxygen first
doxygen docs/doxygen/Doxyfile

# 2. Copy the results INTO the MkDocs source folder BEFORE MkDocs runs
rm -rf docs/docs/api
cp -r docs/doxygen/_build/html docs/docs/api

cd docs
# Using the active python venv to build mkdocs
mkdocs build

echo "Docs built at: docs/site/index.html"
