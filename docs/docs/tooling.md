# CLI Usage

Rivet is currently used through the `rivet` command-line compiler.

## Command Shape

```bash
./build/rivet <source_file.rvt> [options]
```

## Core Options

- `--dump-ast`
  - Prints parsed AST for debugging.
- `--target=<triple>`
  - Overrides LLVM target triple.
- `--mcpu=<cpu>`
  - Sets CPU model for backend code generation.
- `--flash-origin=<addr>`
- `--flash-size=<size>`
- `--ram-origin=<addr>`
- `--ram-size=<size>`
  - Memory-layout parameters for embedded-oriented builds.

## Examples

Basic compile:

```bash
./build/rivet tests/validation.rvt
```

Compile and print AST:

```bash
./build/rivet tests/validation.rvt --dump-ast
```

Cross-target compile options:

```bash
./build/rivet tests/validation.rvt \
  --target=arm-none-eabi \
  --mcpu=cortex-m4
```

## Practical Notes

- The compiler prints LLVM IR to stdout as part of normal execution.
- If `main()` exists in your source, Rivet wires it into `__rivet_entry`.
- If no input file is provided, Rivet exits with usage guidance.
