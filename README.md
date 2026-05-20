# Rivet

Rivet is a small, LLVM-backed systems language prototype focused on readable syntax and explicit control. This repository contains the compiler frontend, LLVM IR codegen, tests, and documentation site.

## What Works Today

- Integer, string, reference, and fixed-size array declarations
- Expression statements and assignments
- Arithmetic and comparison operators (`+`, `-`, `*`, `/`, `==`, `!=`, `<`, `>`)
- Keyword operators (`and`, `or`, `lsft`, `rsft`)
- Control flow: `if/else`, `while`, and `for`
- Function call expressions
- Module imports via `import name;`
- LLVM IR generation into a synthetic entry function `__rivet_entry`

## In Progress

- Function declarations (`fun`/`return`) are tokenized but not fully integrated in parsing and codegen.
- Unary lowering is still incomplete.
- Arrays, strings, and references have coverage tests but continue to evolve.

## Quick Example

```rivet
import dummy;

int i = 0;
while (i < 3) {
  i = i + 1;
}

i + imported_x;
```

## Project Layout

- `include/Rivet/` - Public headers for the compiler
- `src/` - Implementations of the compiler modules
- `tests/` - `.rvt` files used for parser and semantic coverage
- `docs/` - MkDocs site sources and Doxygen configuration
- `lib/` - Importable Rivet modules

## Documentation (MkDocs + Doxygen)

The docs site is built with MkDocs, and Doxygen API reference is generated automatically as part of the MkDocs build process.

Build docs locally:

```bash
cd docs
mkdocs build
```

Publish to GitHub Pages:

```bash
cd docs
mkdocs gh-deploy
```

The pre-build hook runs Doxygen and copies the HTML output into `docs/docs/api` before MkDocs renders the site.
