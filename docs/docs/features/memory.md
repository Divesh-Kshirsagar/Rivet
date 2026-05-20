# Memory Model

Rivet models memory explicitly so newcomers can understand how values are stored and accessed. The current implementation focuses on stack allocation and simple references.

## Stack Allocation

When you declare a variable, Rivet allocates a stack slot in LLVM IR:

```rivet
int x = 7;
```

This becomes an `alloca` and a `store` in LLVM, and reads become `load` instructions.

## References

References are created with `address_of` and read with `deref`:

```rivet
int base = 42;
int ref ptr = address_of base;
int value = deref ptr;
```

Use references when you want explicit address manipulation in your program logic.

## Arrays in Memory

Fixed-size arrays are allocated as a block of contiguous integers. Indexing compiles into pointer arithmetic on that block.

```rivet
int[3] flags;
flags[0] = 1;
flags[1] = 0;
```

## Current Limitations

- Only stack allocation is modeled for now.
- There is no heap allocation or dynamic array support yet.
- Null reference handling is still being refined.
