# Project Structure

This page explains how the Rivet repository is organized and where to find the core compiler components.

## Key Directories

- `include/Rivet/` - Public headers for the compiler (AST, Lexer, Parser, SymbolTable, Type, CodeGen).
- `src/` - Implementations of the core compiler modules.
- `tests/` - Sample `.rvt` files used for parser and semantic coverage.
- `docs/` - MkDocs site sources and Doxygen configuration.
- `lib/` - Importable Rivet modules (used by `import`).
- `examples/` - Example programs or experiments.

## Core Files

- `include/Rivet/AST.h` / `src/AST.cpp` - AST node definitions and codegen.
- `include/Rivet/Lexer.h` / `src/Lexer.cpp` - Tokenization and lexing logic.
- `include/Rivet/Parser.h` / `src/Parser.cpp` - Recursive descent parsing logic.
- `include/Rivet/CodeGen.h` / `src/CodeGen.cpp` - LLVM context setup.
- `include/Rivet/SymbolTable.h` - Scope and symbol tracking.
- `include/Rivet/Type.h` - Type system definitions.
- `src/main.cpp` - Compiler driver and entry point.

## API Reference

For a complete class breakdown and dependency graphs, see the [C++ API Reference](../api/index.html). It is generated with Doxygen and includes class hierarchies, include graphs, and symbol indexes.
