OUTPUT_ARCH(riscv)
ENTRY(_start)

BASE_ADDRESS = 0x80000000;

PHDRS
{
    rx PT_LOAD FLAGS(5);
    rw PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = BASE_ADDRESS;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    } :rx

    .rodata : {
        *(.rodata .rodata.*)
    } :rx

    .data : {
        *(.data .data.*)
    } :rw

    .bss : {
        _sbss = .;
        *(.bss .bss.*)
        _ebss = .;
    } :rw

    . = ALIGN(16);
    _stack_start = .;
    . += 0x2000;
    _stack_end = .;

    _end = .;
}
