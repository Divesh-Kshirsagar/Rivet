# LLVM Backend Details

This page explains how Rivet lowers language constructs into LLVM IR in the current implementation.

## Module and Entry Function

Codegen initializes:

- `LLVMContext`
- `IRBuilder<>`
- `Module` named `Rivet Bare Metal Module`

Top-level code is emitted into:

```llvm
define i32 @__rivet_entry() {
entry:
  ...
}
```

## Variables

```rivet
int x = 7;
```

Becomes roughly:

```llvm
%x = alloca i32
store i32 7, ptr %x
```

Variable reads use `load`.

## Binary Operators

Current mapping:

- `+` -> `CreateAdd`
- `-` -> `CreateSub`
- `*` -> `CreateMul`
- `/` -> `CreateSDiv`
- `and` -> `CreateAnd`
- `or` -> `CreateOr`
- `lsft` -> `CreateShl`
- `rsft` -> `CreateAShr`
- `==` -> `icmp eq` then zero-extend to `i32`
- `!=` -> `icmp ne` then zero-extend to `i32`
- `<` -> `icmp slt` then zero-extend to `i32`
- `>` -> `icmp sgt` then zero-extend to `i32`
- `=` -> `store` into variable slot and return RHS value

## Control Flow Lowering

### If / Else

If creates blocks:

- `then`
- `else`
- `ifcont`

The condition is tested against zero for branch selection.

### While

While creates blocks:

- `cond`
- `loop`
- `afterloop`

Flow:

1. branch to `cond`
2. evaluate condition
3. branch to `loop` or `afterloop`
4. loop body branches back to `cond`

### For

`for` lowers to a loop with a dedicated induction variable and a step value. It uses the same structure as `while`, but emits initialization and increment logic around the loop body.

## Strings

Strings are represented as a simple struct containing a pointer and length. The backend builds a `String` struct type in the LLVM context and reuses it for string values.

## Imports and IR

`ImportAST::codegen()` emits all imported nodes before continuing with the importing file. This effectively injects imported declarations and statements into the same module.

## Current Backend Limitations

- Unary lowering is not fully implemented yet.
- Function declarations are not yet integrated into parser and codegen.
- Some edge cases in array and reference semantics are still being refined.
