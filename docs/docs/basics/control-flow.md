# Control Flow

Rivet supports block-based `if/else`, `while`, and `for` loops. All control-flow bodies are parsed as blocks (`{ ... }`).

## `if` / `else`

```rivet
if (condition) {
  // statements
} else {
  // statements
}
```

Notes:

- Conditions must be wrapped in parentheses.
- `else` is optional.
- In codegen, conditions are treated as integers: zero is false, non-zero is true.

## `while`

```rivet
while (condition) {
  // statements
}
```

The loop re-evaluates the condition before each iteration.

## `for`

Rivet uses a range-style `for` loop:

```rivet
for (i in 0 to 10 step 1) {
  // statements
}
```

Notes:

- `step` is optional; if omitted, a step of `1` is assumed.
- `i` is the loop iterator name.
- `to` is inclusive of the upper bound in current tests.

## Example

```rivet
int i = 0;
int sum = 0;

while (i < 5) {
  if (i == 2) {
    sum = sum + 10;
  } else {
    sum = sum + i;
  }
  i = i + 1;
}
```

## Current Limitations

- `break` and `continue` are not implemented yet.
- Single-line bodies without `{}` are not supported in the parser.
