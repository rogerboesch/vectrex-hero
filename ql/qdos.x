/* vlink linker script for QDOS executable */
/* Produces a flat binary that can be loaded as a QDOS job */

SECTIONS {
    . = 0;
    .text : { *(.text) *(.rodata) *(.rodata.*) }
    .data : { *(.data) *(.data.*) }
    .bss  : {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
}
