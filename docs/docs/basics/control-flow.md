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

Rivet supports two `for` loop forms.

Range-style loop:

```rivet
for (i in 0 to 10 step 1) {
  // statements
}
```

Array-iteration loop:

```rivet
for (pin in pins) {
  // statements
}
```

Notes:

- `step` is optional; if omitted, a step of `1` is assumed.
- The identifier before `in` is always the loop iterator name.
- `to` is inclusive of the upper bound in current tests.
- In array iteration, the iterator receives each element value in order.

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
