# Performance

Rivet is an early-stage compiler, but it already emits direct LLVM IR for core operations. This page explains how to think about performance today.

## What Is Fast Today

- Integer arithmetic and comparisons map directly to LLVM instructions.
- Local variables are stack-allocated and accessed with `load`/`store`.
- Loops compile to simple control-flow blocks.

## Where Overhead Can Appear

- Imports inline all parsed nodes, which can increase compile time for large trees.
- Some semantic checks are conservative and may reject edge cases.
- String handling is still evolving and may change structure in future revisions.

## Practical Tips

- Keep loops simple when testing performance paths.
- Prefer integer math in hot paths.
- Use `--dump-ast` only when debugging; it adds runtime overhead during compilation.

## Roadmap Ideas

Future improvements may include richer optimizations, more aggressive constant folding, and expanded IR analysis. For now, the primary focus is correctness and clarity.
