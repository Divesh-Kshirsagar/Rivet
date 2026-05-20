# Rivet Language Docs

Rivet is a small, LLVM-backed systems language prototype designed for readable syntax and explicit control. This site documents the language as it exists in this repository today, so you can learn what works now and what is still in progress.

## What You Can Do Today

- Declare integers, strings, references, and fixed-size arrays
- Write expressions and assignments (`=`, `+`, `-`, `*`, `/`)
- Use comparisons (`==`, `!=`, `<`, `>`) and keyword operators (`and`, `or`, `lsft`, `rsft`)
- Create blocks, `if/else`, `while`, and `for` loops
- Call functions defined in the LLVM module
- Import other Rivet files with `import name;`
- Generate LLVM IR through a synthetic entry function `__rivet_entry`

## What Is Still Evolving

- Function declarations (`fun`/`return`) are tokenized but not fully integrated in parsing and codegen.
- Unary operators are parsed, but not all forms are lowered yet.
- Some features (arrays, strings, refs) are implemented but still receiving semantic checks and edge-case coverage.
- Import resolution searches:
  - `lib/<module>.rvt`
  - `../lib/<module>.rvt` (fallback when running from build directories)

## Quick Example

```rivet
import dummy;

int i = 0;
while (i < 3) {
  i = i + 1;
}

i + imported_x;
```

Run from `build/`:

```bash
./rivet ../tests/import_dummy_test.rvt --dump-ast
```

## Typical Compile Flow

1. **Lexing**: source is tokenized with line/column tracking.
2. **Parsing**: recursive descent builds AST nodes.
3. **AST dump (optional)**: use `--dump-ast` to inspect the tree.
4. **Codegen**: AST emits LLVM IR into `Rivet Bare Metal Module`.
5. **IR print**: the module is printed to stdout.

## Reading Guide

- Start with **Language Basics** to learn core syntax.
- Use **Features** to see strings, arrays, modules, and memory behavior.
- Use **Reference** for compact grammar and keyword tables.
- Visit **Internals** to understand the parser and LLVM backend flow.

