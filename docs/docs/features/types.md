# Types

Rivet is a small language prototype, but it already includes a few core types that appear throughout the compiler and tests.

## `int`

The primary numeric type is `int`:

```rivet
int count = 10;
int total = count + 5;
```

`int` values are currently represented as 32-bit integers in LLVM IR.

## `str`

Strings are supported through the `str` keyword:

```rivet
str message = "Hello Bare Metal";
str label;

label = "RIVET";
message = label;
```

The backend lowers strings to a small struct containing a pointer and length.

## References (`ref`)

References use the `ref` keyword combined with `address_of` and `deref`:

```rivet
int base = 42;
int ref ptr = address_of base;

int roundtrip = deref ptr;
```

References are useful for modeling memory access and pointer-like behavior in tests.

## Arrays

Rivet supports fixed-size arrays of integers:

```rivet
int[4] nums;
nums[0] = 1;
nums[1] = 2;
```

Array indexing works on both the left and right side of assignments.

## Notes

- `void` exists as a token, but full function signatures are still under construction.
- The type checker continues to expand; some edge cases are still being refined.
