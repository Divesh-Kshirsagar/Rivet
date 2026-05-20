# Modules and Imports

Rivet uses a simple import system that loads other `.rvt` files and injects their AST nodes into the current module.

## Import Syntax

```rivet
import dummy;
```

The parser reads the target module, parses its contents, and places them into an `ImportAST` node. During codegen, imported nodes are emitted before the current file continues.

## Import Resolution Paths

The current implementation searches for module files in:

1. `lib/<module>.rvt`
2. `../lib/<module>.rvt` (fallback when running from build directories)

## Notes

- Imports are cached to avoid duplicate loads.
- Cyclic imports are prevented by tracking module names.
