
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 320K
}

ENTRY(Reset_Handler)

_estack = 0x20050000;    /* end of RAM */

SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) } > FLASH
  .rodata : { *(.rodata*) } > FLASH
  .data : { *(.data*) } > RAM AT > FLASH
  .bss : { *(.bss*) } > RAM
}
