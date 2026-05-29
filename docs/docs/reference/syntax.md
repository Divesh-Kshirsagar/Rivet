# Syntax Reference

This is a compact syntax guide for writing Rivet programs in `v0.0.1`.

## File Structure

A file may contain:

- `import` statements
- variable declarations
- function declarations
- control-flow and expression statements

## Imports

```rivet
import dummy;
```

## Variables

```rivet
int x;
int y = 10;
str message = "Hello";
int ref p = address_of y;
int optref maybe = null;
int[4] nums;
```

Notes:
- `void` is not a variable type.
- `str ref` and `str optref` are not currently supported.

## Functions

```rivet
fun int add(int a, int b) {
  return a + b;
}

fun void nop() {
  return;
}
```

## Control Flow

```rivet
if (x == 0) {
  x = x + 1;
} else {
  x = x + 2;
}

while (x < 10) {
  x = x + 1;
}

for (i in 0 to 10 step 1) {
  x = x + i;
}

for (item in nums) {
  x = x + item;
}
```

Notes:
- `if`, `else`, and `while` bodies must be blocks (`{ ... }`).

## Expressions

Primary forms:

- integer literal
- string literal
- identifier
- function call
- parenthesized expression
- `address_of <identifier>`
- `deref <expression>`
- array index `arr[i]`
- `null`

Operators:

- assignment: `=`
- arithmetic: `+`, `-`, `*`, `/`
- comparison: `<`, `>`, `==`, `!=`
- keyword operators: `and`, `or`, `lsft`, `rsft`
- unary: `-`, `not`

## Statement Endings

- Declarations and expression statements end with `;`
- Block/control structures use `{ ... }`
