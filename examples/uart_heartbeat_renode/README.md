# UART Heartbeat Renode Example

This example is a practical frontend/backend validation target that:
- initializes USART2
- prints `tick 00` .. `tick 19`
- prints `[RIVET] heartbeat`

## Files

- `test.rvt`: Rivet source
- `run.resc`: Renode script
- `rivet_arm-none-eabi_cortex-m0.ld`: linker script
- `rivet_arm-none-eabi_cortex-m0_startup.s`: startup/vector table

## Build

From repo root:

```bash
cd examples/uart_heartbeat_renode
../../build/rivet test.rvt --target=arm-none-eabi --mcpu=cortex-m0
```

Rivet will emit:
- `output.o`
- `rivet_arm-none-eabi_cortex-m0_startup.s`
- `rivet_arm-none-eabi_cortex-m0_startup.o`
- `rivet_arm-none-eabi_cortex-m0.ld`
- `firmware.elf`

## Run in Renode

```bash
renode run.resc
start
```

You should see tick lines and a final heartbeat message in the UART2 analyzer.
