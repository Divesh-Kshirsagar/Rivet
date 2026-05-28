# Rivet Language Docs

Welcome to the user documentation for **Rivet v0.0.1**.

If you want to write programs in Rivet today, this site shows what works, how to run it, and where current limits are.

## Start Here

1. Go to **Getting Started** for build + first run.
2. Read **Language Basics** for syntax you will use most.
3. Use **Reference** when you need a quick rule or keyword lookup.

## What You Can Build Today

- Integer and string variables
- Fixed-size integer arrays
- References and optional references (`ref`, `optref`, `address_of`, `deref`, `null`)
- Expressions with arithmetic, comparisons, and keyword operators (`and`, `or`, `lsft`, `rsft`)
- Control flow with `if/else`, `while`, and `for`
- Functions with `fun` and `return`
- Multi-file programs with `import module;`

## Quick Example

```rivet
fun int main() {
  int sum = 0;

  for (i in 0 to 4 step 1) {
    sum = sum + i;
  }

  return sum;
}
```

## Current Limits You Should Know

- Error messages are improving, but some are still terse.
- Imports are path-convention based (`lib/` and `../lib/` fallback).
- Some edge cases in arrays/refs/strings are still under active validation.

## Version

This documentation targets **Rivet v0.0.1**.
