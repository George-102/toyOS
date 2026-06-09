;Multiboot 引导代码

MULTIBOOT_HEADER_MAGIC equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS equ 0x00000003
CHECKSUM equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd  MULTIBOOT_HEADER_MAGIC
    dd  MULTIBOOT_HEADER_FLAGS
    dd  CHECKSUM

;data section
section .text 
global _start
extern main

_start:
    cli
    mov esp, 0x300000

    call main

    hang:
        hlt
        jmp hang
        
