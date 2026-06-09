OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }
    .data : ALIGN(4K)
    {
        *(.data)
    }
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    . = 0x300000;
    _stack_top = .;
}