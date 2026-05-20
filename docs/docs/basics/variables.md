# Variables

This page introduces variables as implemented in Rivet today. You will see how to declare values, assign them, and work with arrays and references.

## Integer Variables

```rivet
int x;
int y = 42;
```

- `int x;` is valid and defaults to `0` in codegen.
- `int y = <expression>;` evaluates the expression and stores the result.
- Declarations end with `;`.

## String Variables

```rivet
str message = "Hello Bare Metal";
str label;

label = "RIVET";
message = label;
```

Strings are first-class values in the current prototype. Assignments between `str` values are allowed.

## Fixed-Size Arrays

Rivet supports fixed-size arrays of integers using bracket syntax:

```rivet
int[5] nums;
nums[0] = 10;
nums[1] = nums[0] + 5;
```

Array indexing works in expressions and assignments. Index expressions must resolve to integers.

## References (Address and Dereference)

Rivet models references using `address_of` and `deref`:

```rivet
int base = 42;
int ref ptr = address_of base;

int roundtrip = deref ptr;
```

Use `int ref` to declare a reference. The `deref` operator reads the value at the referenced location.

## Assignment Rules

Assignments are binary expressions with `=`:

```rivet
int i = 0;
i = i + 1;
```

Current rule: the left-hand side must be a declared variable or an array element.

## Scope Notes (Current Implementation)

Variables are tracked in a shared name map for codegen and semantic checks.
For now, keep names unique within the same block to avoid accidental shadowing.
