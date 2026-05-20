# Low-level Features

Rivet exposes a few low-level constructs so you can experiment with memory and bitwise behavior while keeping syntax readable.

## Bitwise and Logical Operators

Rivet uses keyword operators for bitwise and logical actions:

- `and`
- `or`
- `lsft`
- `rsft`

Example:

```rivet
int flags = 1;
flags = flags lsft 2;
flags = flags or 3;
```

## References and Explicit Addressing

```rivet
int base = 10;
int ref ptr = address_of base;
int value = deref ptr;
```

These are useful for modeling pointer-like operations and testing dereference semantics.

## Array Indexing

Array indexing compiles into offset calculations over a contiguous integer block:

```rivet
int[3] nums;
nums[0] = 7;
nums[1] = nums[0] + 1;
```

## Current Caveats

- There is no raw pointer type beyond `ref` constructs.
- No heap allocation or manual memory release is available yet.
- Some semantic checks are still strict or incomplete.
