# AST/IR Manual Inspection Suite

This suite is frontend-focused and intended for manual validation by inspecting:
- AST output (`--dump-ast`)
- emitted LLVM IR
- compiler diagnostics on stderr

## Structure

- `syntax/`: parser and expression-shape coverage
- `semantics/`: symbol/scope/function semantic behavior
- `types/`: type/ref/array/null behavior
- `imports/`: module import behavior
- `intrinsics/`: compiler intrinsic behavior

Each test has:
- `<name>.rvt`: source program
- `<name>.md`: checklist for manual review

## Run One Case

```bash
./build/rivet tests/ast_ir/syntax/control_flow_and_loops.rvt --dump-ast
```

## Run All Cases (Capture Logs)

```bash
./scripts/run_ast_ir_suite.sh
```

The runner captures logs into `tests/out/` and does not enforce pass/fail assertions.
