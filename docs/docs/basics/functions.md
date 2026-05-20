# Functions

Rivet currently supports function call expressions. Function declarations exist as tokens, but the full declaration pipeline is still in progress.

## Calling Functions

A function call is parsed when an identifier is followed by `(`:

```rivet
do_work();
add(2, 3);
```

Arguments are comma-separated expressions. During codegen, Rivet:

1. Looks up the callee by name in the LLVM module.
2. Verifies the argument count.
3. Emits an LLVM `call` instruction.

If the callee is missing or the arity does not match, codegen reports an error.

## Function Declarations (Planned)

Tokens for `fun` and `return` exist, but parsing and codegen for function declarations are not fully integrated yet. Consider function definitions a planned feature while the current focus is on top-level statements and the synthetic `__rivet_entry` function.

## Example Call Form

```rivet
import math;

int x = 7;
int y = 3;
add(x, y);
```
