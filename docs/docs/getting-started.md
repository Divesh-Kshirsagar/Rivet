# Getting Started

This guide is for developers who want to write and run Rivet programs right now.

## Quick Install (Prebuilt Binary)

Latest release:

```bash
curl -fsSL https://raw.githubusercontent.com/diveshkshirsagar/RIVET/main/install.sh | bash
```

Specific version:

```bash
curl -fsSL https://raw.githubusercontent.com/diveshkshirsagar/RIVET/main/install.sh | RIVET_VERSION=v0.0.1 bash
```

Default install target is `/usr/local/bin`, so `rivet` is available as a normal shell command.

## Prerequisites

- CMake `>= 3.16`
- A C++17 compiler
- LLVM installed with CMake package config available

## Build the Compiler

From repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

This creates the compiler binary at `build/rivet`.

## Your First Program

Create a file like `hello.rvt`:

```rivet
fun int main() {
  int a = 2;
  int b = 3;
  return a + b;
}
```

Run it:

```bash
./build/rivet hello.rvt
```

## Useful Run Modes

Print AST while compiling:

```bash
./build/rivet hello.rvt --dump-ast
```

Compile using explicit target settings:

```bash
./build/rivet hello.rvt \
  --target=arm-none-eabi \
  --mcpu=cortex-m0
```

Optional memory layout flags:

```bash
./build/rivet hello.rvt \
  --flash-origin=0x08000000 \
  --flash-size=512K \
  --ram-origin=0x20000000 \
  --ram-size=128K
```

## Typical Workflow

1. Write `.rvt` file.
2. Run `./build/rivet <file>.rvt`.
3. If needed, rerun with `--dump-ast` for debugging syntax/structure.
4. Iterate using examples in `tests/` and `examples/`.

## Where to Go Next

- **Language Basics** for variables, control flow, and functions.
- **Features** for memory, modules, and types.
- **Reference** for syntax/keywords lookup.
