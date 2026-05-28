# Cortex-M0 Simulation

This page describes the current demo assets for embedded-style experimentation.

## What Is Included

Under `examples/basic_test/` and `examples/uart_test/`, the repo includes:

- linker scripts
- startup assembly
- generated objects/ELF artifacts
- Renode run scripts (`run.resc`)

## Typical Flow

1. Compile your Rivet program with `./build/rivet <file>.rvt`.
2. Use your target toolchain/linker flow with the provided linker/startup assets.
3. Run simulation through the Renode script in the example folder.

## Note

The exact simulation command may depend on your local Renode setup. Use the included `run.resc` files as the baseline entry point.
