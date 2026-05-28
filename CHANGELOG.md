# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.0.1] - 2026-05-28

### Added

- LLVM-backed compiler pipeline wired end-to-end: lexing, parsing, semantic checks, AST-driven codegen, and target setup.
- Lexer support for keywords, identifiers, integer literals, string literals, operators/punctuation, and both line/block comments.
- Parser support for:
  - variable declarations (`int`, `str`, references, optional references, fixed-size arrays)
  - expressions and assignments
  - control flow (`if/else`, `while`, `for`)
  - function declarations (`fun`) and `return`
  - imports (`import module;`)
  - memory-oriented constructs (`address_of`, `deref`, `null`, `__volatile_store` intrinsic)
- Semantic analysis with scope-aware symbol handling, function signature registration pass, and program-wide type-checking pass.
- LLVM IR emission through a synthetic `__rivet_entry` function with call-through to user `main()` when available.
- Test programs and examples for validation, including embedded-oriented example artifacts and Renode-oriented files.
- Documentation toolchain wiring for MkDocs + Doxygen.

### Changed

- Compiler driver now follows explicit multi-pass ordering for function correctness:
  - semantic pass 1: function signature registration
  - semantic pass 2: full type-check
  - codegen pass 1: function prototype creation
  - codegen pass 2: full IR generation
- Entrypoint behavior formalized so top-level execution path is finalized through `__rivet_entry`.
- Import resolution behavior includes fallback path support when compiling from `build/`-style directories.

### Known Limitations

- Diagnostics are functional but still evolving in quality and consistency.
- Some language edges (especially around strings/arrays/references/unary combinations) require broader regression coverage.
- Import resolution is currently convention-based (`lib/<module>.rvt` with `../lib/` fallback), not yet configurable.
- Versioning in this release is documentation-aligned (`0.0.1`) and does not yet imply all build metadata is synchronized.
