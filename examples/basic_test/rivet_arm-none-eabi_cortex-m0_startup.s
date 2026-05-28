.syntax unified
.cpu cortex-m0
.thumb

.global vtable
.global Reset_Handler

.section .isr_vector,"a",%progbits
vtable:
    .word 0x20020000  /* Initial SP: top of RAM (0x20000000 + 128K) */
    .word Reset_Handler                          /* Reset Vector */

.section .text.Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    bl __rivet_entry
hang:
    b hang

.section .note.GNU-stack,"",%progbits
