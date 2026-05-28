# Functions

Rivet supports user-defined functions using `fun` and `return`.

## Declaration Form

```rivet
fun int add(int a, int b) {
  return a + b;
}
```

Return type can be `int`, `str`, or `void`.

## Calling Functions

```rivet
fun int add(int a, int b) {
  return a + b;
}

fun int main() {
  int result = add(2, 3);
  return result;
}
```

Arguments are comma-separated expressions.

## Parameters

Supported parameter forms include:

- Plain values: `int x`, `str name`
- References: `int ref p`
- Optional references: `int optref maybe`

## Return Statements

```rivet
fun int identity(int x) {
  return x;
}

fun void log_value(int x) {
  return;
}
```

## Current Limits

- Function overloading is not supported.
- Type conversions are not implicit; argument and return types must match expected forms.
