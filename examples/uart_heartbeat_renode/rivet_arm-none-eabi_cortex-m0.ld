ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
  .text : {
    KEEP(*(.isr_vector))  /* Vector table must be first */
    *(.text*)
    *(.rodata*)
  } > FLASH
  /* ARM exception index table.                                         */
  /* __exidx_start/_end are required by libgcc's unwinder (unwind-arm) */
  __exidx_start = .;
  .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH
  __exidx_end = .;
  .data : { *(.data*) } > RAM AT > FLASH
  .bss  : { *(.bss*) *(COMMON) } > RAM
  /* Heap boundary symbols required by newlib's _sbrk (libnosys) */
  end = .;
  _end = .;
  __end__ = .;
}
