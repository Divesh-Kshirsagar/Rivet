# Internals Overview

This page describes the current compiler pipeline as implemented in this repository. It is intended for readers who want to connect the language features to the codebase.

## High-Level Architecture

Rivet uses a classic frontend + LLVM backend structure:

1. **Lexer** (`src/Lexer.cpp`)
2. **Parser** (`src/Parser.cpp`)
3. **AST** (`include/Rivet/AST.h`, `src/AST.cpp`)
4. **Codegen Context** (`include/Rivet/CodeGen.h`, `src/CodeGen.cpp`)
5. **Driver** (`src/main.cpp`)

## Lexer

The lexer reads a file stream and emits integer tokens:

- Keywords are negative enum values (for example `tok_if`, `tok_import`).
- Single-character operators and punctuation are returned as ASCII values.
- Line and column are tracked for diagnostics.
- Line comments (`//`) are supported.

## Parser

The parser is recursive descent with a precedence-aware binary expression parser.

### Statement entry points

Current statements include:

- `import <id>;`
- variable declarations (`int`, `str`, `int ref`, arrays)
- `if (...) { ... } [else { ... }]`
- `while (...) { ... }`
- `for (i in start to end [step step])`
- `for (item in arrayName)`
- blocks `{ ... }`
- expression statements
- empty statement `;`

### Expression parsing

Primary expressions include:

- number literals
- string literals
- identifiers and calls
- parenthesized expressions

The `ParseBinOpRHS` function applies operator precedence for binary expressions.

### Import flow

`ParseImport` resolves the module name, avoids duplicate imports, and parses the imported file into an `ImportAST` node. Import search paths currently include `lib/<module>.rvt` and `../lib/<module>.rvt`.

## AST Layer

Key AST nodes include:

- literals and variables (`NumberAST`, `StringLiteralAST`, `VariableAST`)
- declarations (`VariableDeclAST`)
- operators (`BinaryOpAST`, `UnaryOpAST`)
- control flow (`IfAST`, `WhileAST`, `ForAST`, `BlockAST`)
- calls and imports (`CallAST`, `ImportAST`)
- memory nodes (`AddressOfAST`, `DerefAST`, `IndexAST`)

Each node provides `dump()` for debug output and `codegen()` for LLVM emission.

## Driver (`main`)

The driver:

1. Loads the source file.
2. Parses to a top-level AST node list.
3. Optionally dumps the AST.
4. Initializes the LLVM module context.
5. Builds a synthetic entry function `__rivet_entry`.
6. Codegens each top-level node.
7. Emits a return if the block lacks a terminator.
8. Prints the full LLVM IR.
