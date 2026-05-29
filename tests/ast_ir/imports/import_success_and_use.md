# Checklist

- Import should load `mathx` fixture module successfully.
- AST should include `ImportAST` with imported function body nodes.
- IR should include callable imported function and valid call from `main`.
- Run this from `tests/fixtures` cwd (or runner import mode) so `lib/mathx.rvt` resolves.
