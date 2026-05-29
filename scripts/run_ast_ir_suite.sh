#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RIVET_BIN="${RIVET_BIN:-$ROOT_DIR/build/rivet}"
SUITE_DIR="$ROOT_DIR/tests/ast_ir"
OUT_DIR="$ROOT_DIR/tests/out"

mkdir -p "$OUT_DIR"

if [[ ! -x "$RIVET_BIN" ]]; then
  echo "rivet binary not found or not executable: $RIVET_BIN"
  echo "Build first: cmake -S . -B build && cmake --build build -j"
  exit 1
fi

echo "Running AST/IR manual suite"
echo "Binary: $RIVET_BIN"
echo "Output dir: $OUT_DIR"
echo

mapfile -t cases < <(find "$SUITE_DIR" -type f -name "*.rvt" | sort)

if [[ ${#cases[@]} -eq 0 ]]; then
  echo "No .rvt cases found under $SUITE_DIR"
  exit 0
fi

for case_file in "${cases[@]}"; do
  rel="${case_file#$ROOT_DIR/}"
  slug="${rel//\//__}"
  base="${slug%.rvt}"

  out_log="$OUT_DIR/${base}.ast_ir.log"
  err_log="$OUT_DIR/${base}.err.log"

  echo "=== $rel ==="
  if [[ "$rel" == tests/ast_ir/imports/* ]]; then
    (
      cd "$ROOT_DIR/tests/fixtures" || exit 1
      "$RIVET_BIN" "../../${rel}" --dump-ast
    ) >"$out_log" 2>"$err_log"
  else
    "$RIVET_BIN" "$case_file" --dump-ast >"$out_log" 2>"$err_log"
  fi
  echo "  stdout+IR+AST: ${out_log#$ROOT_DIR/}"
  echo "  stderr:        ${err_log#$ROOT_DIR/}"
  echo
 done

echo "Completed. Review logs under tests/out/."
