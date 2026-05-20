# Examples

This page provides small, focused Rivet programs you can read in a few minutes. Each example is written to be clear for first-time readers.

## Hello Expressions

```rivet
int a = 10;
int b = 4;

a + b * 2;
```

## If / Else

```rivet
int value = 7;

if (value == 7) {
  value = value + 1;
} else {
  value = value - 1;
}
```

## While Loop

```rivet
int i = 0;

while (i < 5) {
  i = i + 1;
}
```

## For Loop

```rivet
int sum = 0;

for (i in 0 to 5 step 1) {
  sum = sum + i;
}
```

## Arrays

```rivet
int[4] nums;
nums[0] = 1;
nums[1] = 2;
nums[2] = 3;
nums[3] = 4;

int total = nums[0] + nums[1] + nums[2] + nums[3];
```

## Strings

```rivet
str message = "Hello Bare Metal";
str label;

label = "RIVET";
message = label;
```

## References and Dereference

```rivet
int base = 42;
int ref ptr = address_of base;

int roundtrip = deref ptr;
```

## Imports

```rivet
import dummy;

imported_x + imported_y;
```
