# Examples

Short examples you can run as starting points.

## 1. Function + Return

```rivet
fun int main() {
  int a = 10;
  int b = 20;
  return a + b;
}
```

## 2. If / Else

```rivet
fun int main() {
  int value = 7;

  if (value == 7) {
    value = value + 1;
  } else {
    value = value - 1;
  }

  return value;
}
```

## 3. While Loop

```rivet
fun int main() {
  int i = 0;
  int sum = 0;

  while (i < 5) {
    sum = sum + i;
    i = i + 1;
  }

  return sum;
}
```

## 4. For Range Loop

```rivet
fun int main() {
  int sum = 0;

  for (i in 0 to 5 step 1) {
    sum = sum + i;
  }

  return sum;
}
```

## 5. Arrays

```rivet
fun int main() {
  int[4] nums;
  nums[0] = 1;
  nums[1] = 2;
  nums[2] = 3;
  nums[3] = 4;

  return nums[0] + nums[1] + nums[2] + nums[3];
}
```

## 6. References and Optional References

```rivet
fun int main() {
  int base = 42;
  int ref ptr = address_of base;
  int optref maybe = null;

  return deref ptr;
}
```

## 7. Import + Intrinsic

```rivet
import dummy;

fun int main() {
  __volatile_store(0x40021000, 1);
  return imported_x;
}
```
