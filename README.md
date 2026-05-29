# Rivet

Rivet is an LLVM-backed systems language prototype focused on explicit control, simple syntax, and embedded-oriented workflows.

Current Version: `0.0.1`

## Quick Install (Prebuilt Release)

Install latest release:

```bash
curl -fsSL https://raw.githubusercontent.com/diveshkshirsagar/RIVET/main/install.sh | bash
```

Install a specific version:

```bash
curl -fsSL https://raw.githubusercontent.com/diveshkshirsagar/RIVET/main/install.sh | RIVET_VERSION=v0.0.1 bash
```

Install to a custom directory:

```bash
curl -fsSL https://raw.githubusercontent.com/diveshkshirsagar/RIVET/main/install.sh | RIVET_INSTALL_DIR="$HOME/bin" bash
```

By default, installer targets `/usr/local/bin` so you can run `rivet` directly.

## Implementation Status (v0.0.1)

### Implemented

- **Frontend pipeline:** file-based lexing, recursive-descent parsing, semantic analysis, and LLVM IR generation.
- **Lexer:**
  - Tracks line/column for diagnostics.
  - Supports identifiers, keywords, integers, string literals, operators, punctuation.
  - Supports both `//` and `/* ... */` comments.
- **Parser coverage:**
  - Variable declarations: `int`, `str`, `int ref`, `int optref`, fixed-size `int[N]` arrays.
  - Expressions: literals, identifiers, calls, indexing, assignment, arithmetic (`+`, `-`, `*`, `/`), comparisons (`<`, `>`, `==`, `!=`), keyword ops (`and`, `or`, `lsft`, `rsft`), unary (`-`, `not`), `address_of`, `deref`, `null`.
  - Statements: expression statements, empty `;`, blocks, `if/else` (block bodies), `while` (block body), `for`.
  - Functions: `fun`, typed return (`int`, `str`, `void`), typed parameters, `return`.
  - Imports: `import module;` with duplicate-import guard.
  - Intrinsic: `__volatile_store(address, value)`.
- **Semantic/type analysis:**
  - Scope-aware symbol table checks.
  - Function signature registration pass + full type-check pass.
  - Type matching across declarations, assignments, binary/unary expressions, returns.
  - Rules for refs/optrefs and `null` compatibility.
- **LLVM codegen:**
  - Multi-pass function lowering (prototype creation + body generation).
  - Synthetic entry function `__rivet_entry` generated and wired to user `main()` when present.
  - Module IR printed to stdout.
  - Target/object emission path with LLVM target machine setup.

### Known Limitations / Evolving Areas

- Diagnostics are improving but still basic in some parser/codegen failure paths.
- Some feature edges (strings, arrays, references, unary/operator combinations) need broader regression coverage.
- Import resolution is currently file-path based (`lib/<name>.rvt`, fallback `../lib/<name>.rvt`).
- Tooling and docs sections exist, but deeper language/reference coverage is still expanding.

## Prerequisites

- CMake `>= 3.16`
- C++17-compatible compiler
- LLVM development package discoverable by CMake (`find_package(LLVM REQUIRED CONFIG)`)

## Build

From repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

This builds the `rivet` executable in `build/`.

## Run

Basic invocation:

```bash
./build/rivet tests/validation.rvt
```

Dump AST:

```bash
./build/rivet tests/validation.rvt --dump-ast
```

Target and memory layout flags:

```bash
./build/rivet tests/validation.rvt \
  --target=arm-none-eabi \
  --mcpu=cortex-m0 \
  --flash-origin=0x08000000 \
  --flash-size=512K \
  --ram-origin=0x20000000 \
  --ram-size=128K
```

## Language Snapshot

```rivet
fun int main() {
    int a = 10;
    int b = 20;
    int result = 0;

    if (a == 10) {
        result = a + b;
    } else {
        result = a - b;
    }

    return result;
}
```

```rivet
int base = 42;
int ref p = address_of base;
int optref maybe = null;
int value = deref p;
```

```rivet
import dummy;
__volatile_store(0x40021000, 1);
```

## Compiler Architecture

Rivet currently follows this pipeline:

1. `Lexer` tokenizes input with source-position tracking.
2. `Parser` builds AST with precedence-aware expression parsing.
3. Semantic phase 1 registers function signatures.
4. Semantic phase 2 validates full program typing/scopes.
5. Codegen phase 1 creates function prototypes in LLVM module.
6. Codegen phase 2 emits function and statement IR.
7. Driver finalizes `__rivet_entry`, prints IR, configures target emission.

## Repository Layout

- `src/` - compiler implementation (`main`, lexer, parser, semantic pass, codegen)
- `include/Rivet/` - public compiler headers and AST/type interfaces
- `tests/` - Rivet source programs for validation and feature coverage
- `examples/` - example outputs/integration artifacts (including Renode-related samples)
- `docs/` - MkDocs + Doxygen docs source and generated site assets
- `scripts/` - helper scripts (including docs build flow)

## Testing and Validation

Frontend validation is source-program driven and AST/IR-inspection oriented:

- `tests/validation.rvt`
- `tests/ast_ir/` (comprehensive manual-inspection suite)

Run a representative test file:

```bash
./build/rivet tests/validation.rvt --dump-ast
```

Run the full manual AST/IR suite and capture logs:

```bash
./scripts/run_ast_ir_suite.sh
```

Review logs under `tests/out/` and use `tests/ast_ir/GAPS.md` to track semantic/type-check gaps discovered during review.

## Documentation

Documentation tooling lives under `docs/`.

Build docs locally:

```bash
./scripts/docs.sh
```

The script runs Doxygen first, copies API output into MkDocs source, then builds the MkDocs site.

## Release Packaging

Create a local prebuilt release artifact:

```bash
./scripts/package_release.sh 0.0.1
```

This generates a platform-specific archive in `dist/` named like:

- `rivet-v0.0.1-linux-x86_64.tar.gz`
- `rivet-v0.0.1-macos-arm64.tar.gz`
