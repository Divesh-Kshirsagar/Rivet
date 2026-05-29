# Checklist

- AST contains `FunctionAST` nodes for `add`, `apply`, `nop`, `main` and `CallAST` nodes.
- IR includes separate functions plus call instructions from `main -> nop` and `apply -> add`.
- `void` function return is accepted; no diagnostics expected.
