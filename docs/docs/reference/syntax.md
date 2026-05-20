# Syntax Reference

This page summarizes the current Rivet syntax in a compact, practical form.

## Program Shape

A file can contain top-level items such as:

- import statements
- variable declarations
- control-flow statements
- expression statements

## Statements

### Import

```rivet
import dummy;
```

### Variable declaration

```rivet
int x;
int y = 10;
str message = "Hello";
int ref ptr = address_of x;
int[4] nums;
```

### If / Else

```rivet
if (x == 0) {
  x = x + 1;
} else {
  x = x + 2;
}
```

### While

```rivet
while (x < 10) {
  x = x + 1;
}
```

### For

```rivet
for (i in 0 to 10 step 1) {
  x = x + i;
}
```

### Expression statement

```rivet
x + y;
call_me(x);
```

### Empty statement

```rivet
;
```

## Expressions

Primary forms:

- number literal
- string literal
- identifier
- function call
- parenthesized expression
- `address_of` / `deref`
- array indexing (`nums[i]`)

Supported binary operators:

- assignment: `=`
- relational: `<`, `>`
- equality: `==`, `!=`
- arithmetic: `+`, `-`, `*`, `/`
- keyword operators: `and`, `or`, `lsft`, `rsft`

## Calls

```rivet
sum(a, b, 3);
```

Arguments are comma-separated expressions.

## Notes on Current Parsing Rules

- Conditions for `if`/`while` require parentheses.
- Bodies are currently handled as block forms only.
- Every declaration/statement ends with `;` except block/control structures.
