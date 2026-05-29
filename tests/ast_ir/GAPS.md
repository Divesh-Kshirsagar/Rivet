# Semantic/Typecheck Gaps Log

Use this file while reviewing `tests/ast_ir/*` outputs.

For each discovered gap, append:

- Date:
- Test case:
- Observed behavior:
- Expected behavior:
- Suspected component (lexer/parser/sema/codegen):
- Follow-up action:

## Entries

- Date: 2026-05-29
- Test case: `tests/ast_ir/syntax/expression_precedence.rvt`
- Observed behavior: compiler prints `Unknown unary operator: -22`.
- Expected behavior: `not <expr>` should lower correctly in codegen.
- Suspected component (lexer/parser/sema/codegen): codegen (`UnaryOpAST::codegen`).
- Follow-up action: FIXME added near unknown-unary fallback in `src/AST.cpp`.

- Date: 2026-05-29
- Test case: `tests/ast_ir/types/refs_optrefs_null_valid.rvt`
- Observed behavior: parser reports `Expected 'identifier' after type in variable declaration` for `int ref` / `int optref` declarations.
- Expected behavior: declarations should parse and proceed to semantic/type checks.
- Suspected component (lexer/parser/sema/codegen): parser (`ParseVariableDeclaration` token flow).
- Follow-up action: TODO added in `src/Parser.cpp` near `tok_ref`/`tok_optref` handling.
