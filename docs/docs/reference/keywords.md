# Keywords

This page lists Rivet language keywords you can use in source files in `v0.0.1`.

## Types and Values

- `int`
- `str`
- `void`
- `ref`
- `optref`
- `null`

## Functions and Modules

- `fun`
- `return`
- `import`

## Control Flow

- `if`
- `else`
- `while`
- `for`
- `in`
- `to`
- `step`

## Operators (Keyword Form)

- `and`
- `or`
- `not`
- `lsft`
- `rsft`

## Memory Helpers

- `address_of`
- `deref`

## Notes

- `and`, `or`, `lsft`, and `rsft` are binary operators.
- `not` and `-` are unary operators.
- `void` is used as a function return type, not as a variable type.
- `__volatile_store(address, value)` is available as a compiler intrinsic call form.
