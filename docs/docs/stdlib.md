# Standard Library

Rivet `v0.0.1` does not ship with a stable standard library yet.

## What Exists Today

- Module-style source reuse through `import <module>;`
- Project-local library files typically placed in `lib/`
- Compiler intrinsic call support for low-level operations such as:
  - `__volatile_store(address, value)`

## Recommended Practice

- Keep reusable helpers in small `lib/*.rvt` modules.
- Import only what you need per program.
- Treat module APIs as evolving while the language stabilizes.

This page will be expanded once standard library modules are formalized.
