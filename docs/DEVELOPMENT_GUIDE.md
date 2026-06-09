# toyOS 开发流程指南

本文档提供了从零开始构建 toyOS 的详细步骤指南，按照 [ARCHITECTURE.md](ARCHITECTURE.md) 中定义的四个阶段逐步推进。

---

## 📋 前置准备

### 1. 环境检查

在 WSL 中执行以下命令，确保所有工具已安装：

```bash
# 检查编译器
gcc --version
nasm --version
ld --version

# 检查 QEMU
qemu-i386 --version

# 检查 GRUB 工具
grub-mkrescue --version
```

如果缺少任何工具，执行：
```bash
sudo apt update
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin grub-common xorriso
```

### 2. 项目初始化

```bash
cd /home/george/Code/toyOS

# 创建目录结构
mkdir -p boot kernel include utils docs

# 初始化 Git 仓库（可选）
git init
echo "*.o\n*.bin\n*.iso\nisodir/" > .gitignore
```

---

## 🚀 第一阶段：Hello World (预计 2-4 小时)

### 目标
成功引导内核并在 VGA 屏幕上显示 "Hello, toyOS!"

### 步骤 1.1：创建类型定义头文件

**文件**: `include/types.h`

```c
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

#endif
```

### 步骤 1.2：编写 Multiboot 引导代码

**文件**: `boot/boot.asm`

```nasm
; ============================================================================
; Multiboot 引导代码
; 遵循 Multiboot 规范，使 GRUB 能够加载我们的内核
; ============================================================================

; Multiboot 魔数和标志
MULTIBOOT_HEADER_MAGIC  equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS  equ 0x00000003
CHECKSUM                equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

; Multiboot 头必须在前 8KB 内，且 4 字节对齐
section .multiboot
align 4
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd CHECKSUM

; 代码段
section .text
global _start
extern main

_start:
    ; 禁用中断（保护模式下需要重新设置 IDT 后才能启用）
    cli
    
    ; 设置堆栈指针
    ; 根据 linker.ld，堆栈从 0x300000 开始向下增长
    mov esp, 0x300000
    
    ; 调用 C 语言入口函数
    ; 此时 eax = 0x2BADB002 (Multiboot 魔数)
    ; ebx 指向 Multiboot 信息结构体（后续可用于内存检测）
    call main
    
    ; 如果 main 返回，进入死循环（不应该发生）
    hang:
        hlt
        jmp hang
```

### 步骤 1.3：创建链接器脚本

**文件**: `linker.ld`

```ld
/* 告诉链接器输出格式为 32 位 ELF */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

/* 物理地址和虚拟地址相同（暂未启用分页） */
SECTIONS
{
    /* 内核从 1MB 处开始加载（Multiboot 规范） */
    . = 0x100000;
    
    /* 代码段：包含所有 .text 节 */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }
    
    /* 只读数据段 */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }
    
    /* 数据段：包含初始化的全局变量 */
    .data : ALIGN(4K)
    {
        *(.data)
    }
    
    /* BSS 段：未初始化的全局变量（自动清零） */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
    
    /* 堆栈区域从 3MB 开始 */
    . = 0x300000;
    _stack_top = .;
}
```

### 步骤 1.4：实现 VGA 输出驱动

**文件**: `kernel/io.h`

```c
#ifndef IO_H
#define IO_H

#include "../include/types.h"

// VGA 文本缓冲区起始地址
#define VGA_BUFFER_ADDRESS 0xB8000
// VGA 屏幕尺寸
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// 颜色属性
#define COLOR_BLACK     0x0
#define COLOR_BLUE      0x1
#define COLOR_GREEN     0x2
#define COLOR_CYAN      0x3
#define COLOR_RED       0x4
#define COLOR_MAGENTA   0x5
#define COLOR_BROWN     0x6
#define COLOR_WHITE     0x7

// 串口 COM1 端口
#define SERIAL_COM1_BASE 0x3F8

/**
 * 在 VGA 屏幕上打印字符串
 * @param str 要打印的字符串
 */
void print_string(const char *str);

/**
 * 在 VGA 屏幕上打印带颜色的字符串
 * @param str 要打印的字符串
 * @param color 颜色属性（高 4 位背景色，低 4 位前景色）
 */
void print_color(const char *str, uint8_t color);

/**
 * 清空屏幕
 */
void clear_screen(void);

/**
 * 初始化串口 COM1
 */
void serial_init(void);

/**
 * 通过串口发送一个字符
 * @param c 要发送的字符
 */
void serial_putchar(char c);

/**
 * 通过串口打印字符串
 * @param str 要打印的字符串
 */
void serial_print(const char *str);

#endif
```

**文件**: `kernel/io.c`

```c
#include "io.h"

// VGA 缓冲区指针
static uint16_t *vga_buffer = (uint16_t *)VGA_BUFFER_ADDRESS;
// 当前光标位置
static int cursor_x = 0;
static int cursor_y = 0;

/**
 * 更新硬件光标位置
 */
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    
    // 向 VGA 控制器发送光标位置
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)(0x0F)), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)(0x0E)), "Nd"((uint16_t)0x3D4));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)((pos >> 8) & 0xFF)), "Nd"((uint16_t)0x3D5));
}

/**
 * 在当前位置打印一个字符
 */
static void put_char(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~(8 - 1);
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (color << 8) | c;
        cursor_x++;
    }
    
    // 换行处理
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // 滚动屏幕
    if (cursor_y >= VGA_HEIGHT) {
        // 将下一行内容上移
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        // 清空最后一行
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = (color << 8) | ' ';
        }
        cursor_y = VGA_HEIGHT - 1;
    }
    
    update_cursor();
}

void print_string(const char *str) {
    print_color(str, COLOR_WHITE);
}

void print_color(const char *str, uint8_t color) {
    while (*str) {
        put_char(*str, color);
        str++;
    }
}

void clear_screen(void) {
    uint8_t color = (COLOR_BLACK << 4) | COLOR_WHITE;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (color << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

// ==================== 串口驱动 ====================

void serial_init(void) {
    // 禁用中断
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)(SERIAL_COM1_BASE + 1)));
    
    // 设置波特率除数（115200 波特率）
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x80), "Nd"((uint16_t)(SERIAL_COM1_BASE + 3)));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x03), "Nd"((uint16_t)(SERIAL_COM1_BASE + 0))); // DLL
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)(SERIAL_COM1_BASE + 1))); // DLH
    
    // 设置线路控制寄存器：8 位，无奇偶校验，1 停止位
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x03), "Nd"((uint16_t)(SERIAL_COM1_BASE + 3)));
    
    // 启用 FIFO
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xC7), "Nd"((uint16_t)(SERIAL_COM1_BASE + 2)));
}

void serial_putchar(char c) {
    // 等待发送缓冲区为空
    while ((__asm__ volatile ("inb %1" : "=a"(*(uint8_t*)0) : "Nd"((uint16_t)(SERIAL_COM1_BASE + 5))) & 0x20) == 0);
    
    // 发送字符
    __asm__ volatile ("outb %0, %1" : : "a"(c), "Nd"((uint16_t)SERIAL_COM1_BASE));
}

void serial_print(const char *str) {
    while (*str) {
        serial_putchar(*str);
        str++;
    }
}
```

### 步骤 1.5：编写内核入口点

**文件**: `kernel/main.c`

```c
#include "io.h"

/**
 * 内核主入口函数
 * 由 boot.asm 中的 _start 调用
 */
void main(void) {
    // 初始化串口（用于调试输出）
    serial_init();
    serial_print("[INFO] toyOS kernel started\n");
    
    // 清屏
    clear_screen();
    
    // 打印欢迎信息
    print_color("================================\n", COLOR_GREEN);
    print_color("  Welcome to toyOS v0.0.1      \n", COLOR_GREEN);
    print_color("================================\n", COLOR_GREEN);
    print_color("\nHello, toyOS!\n", COLOR_WHITE);
    print_color("\nSystem initialized successfully.\n", COLOR_CYAN);
    
    serial_print("[INFO] Display output ready\n");
    
    // 进入空闲循环
    while (1) {
        __asm__ volatile ("hlt"); // 暂停 CPU，等待中断
    }
}
```

### 步骤 1.6：创建 Makefile

**文件**: `Makefile`

```makefile
# ============================================================================
# toyOS 构建系统
# ============================================================================

# 编译器配置
CC = gcc
LD = ld
ASM = nasm

# 编译选项
CFLAGS = -m32 -ffreestanding -nostdlib -no-pie -fno-pie \
         -fno-stack-protector -g -O0 -Wall -Wextra
LDFLAGS = -m elf_i386 -T linker.ld
ASMFLAGS = -f elf32

# 目标文件
TARGET = os.iso
KERNEL_BIN = kernel.bin

# 源文件
BOOT_SRC = boot/boot.asm
KERNEL_SRCS = kernel/main.c kernel/io.c

# 对象文件
BOOT_OBJ = boot.o
KERNEL_OBJS = kernel/main.o kernel/io.o

# 默认目标
all: $(TARGET)

# 编译汇编文件
$(BOOT_OBJ): $(BOOT_SRC)
	@echo "[ASM] Compiling $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

# 编译 C 文件
kernel/%.o: kernel/%.c
	@echo "[CC] Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# 链接内核
$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJS)
	@echo "[LD] Linking $@"
	@$(LD) $(LDFLAGS) -o $@ $^

# 生成 ISO 镜像
$(TARGET): $(KERNEL_BIN)
	@echo "[ISO] Creating bootable ISO..."
	@mkdir -p isodir/boot/grub
	@cp $(KERNEL_BIN) isodir/boot/kernel.bin
	@echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	@echo 'menuentry "toyOS" {' >> isodir/boot/grub/grub.cfg
	@echo '    multiboot /boot/kernel.bin' >> isodir/boot/grub/grub.cfg
	@echo '    boot' >> isodir/boot/grub/grub.cfg
	@echo '}' >> isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(TARGET) isodir 2>/dev/null
	@rm -rf isodir
	@echo "[DONE] ISO created: $(TARGET)"

# 运行 QEMU
run: $(TARGET)
	@echo "[QEMU] Starting toyOS..."
	@qemu-i386 -cdrom $(TARGET) -serial stdio

# 运行 QEMU（带 GDB 调试支持）
debug: $(TARGET)
	@echo "[QEMU] Starting toyOS with GDB support..."
	@echo "Connect with: gdb kernel.bin -> target remote :1234"
	@qemu-i386 -cdrom $(TARGET) -s -S -serial stdio

# 清理
clean:
	@echo "[CLEAN] Removing build artifacts..."
	@rm -f *.o kernel/*.o $(KERNEL_BIN) $(TARGET)
	@rm -rf isodir
	@echo "[DONE] Clean complete"

# 查看反汇编
disasm: $(KERNEL_BIN)
	@objdump -d $(KERNEL_BIN) | less

# 帮助
help:
	@echo "toyOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build OS ISO (default)"
	@echo "  run       - Build and run in QEMU"
	@echo "  debug     - Build and run with GDB support"
	@echo "  clean     - Remove build artifacts"
	@echo "  disasm    - View kernel disassembly"
	@echo "  help      - Show this help message"

.PHONY: all run debug clean disasm help
```

### 步骤 1.7：构建并测试

```bash
# 构建项目
make

# 运行测试
make run
```

**预期结果**:
- QEMU 窗口打开
- 黑色背景上显示绿色边框和 "Hello, toyOS!" 字样
- WSL 终端显示串口日志：
  ```
  [INFO] toyOS kernel started
  [INFO] Display output ready
  ```

### ✅ 第一阶段验收检查清单

- [ ] `make` 成功编译无错误
- [ ] QEMU 启动后不黑屏、不重启
- [ ] 屏幕上显示 "Hello, toyOS!"
- [ ] 串口输出内核启动日志
- [ ] 可以使用 `Ctrl+A, X` 退出 QEMU

---

## 🔧 第二阶段：基础架构完善 (预计 6-8 小时)

### 目标
建立 GDT、IDT、PIC，能够捕获和处理异常

### 步骤 2.1：实现全局描述符表 (GDT)

**文件**: `kernel/gdt.h`

```c
#ifndef GDT_H
#define GDT_H

#include "../include/types.h"

// GDT 条目结构
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// GDT 指针结构
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/**
 * 初始化 GDT
 */
void gdt_init(void);

#endif
```

**文件**: `kernel/gdt.c`

```c
#include "gdt.h"
#include "io.h"

// GDT 表（3 个条目：NULL + 代码段 + 数据段）
static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

/**
 * 设置 GDT 条目
 */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, 
                         uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    
    gdt[num].access = access;
}

/**
 * 初始化 GDT
 */
void gdt_init(void) {
    serial_print("[INFO] Initializing GDT...\n");
    
    // 设置 GDT 指针
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (uint32_t)&gdt;
    
    // NULL 段（必须存在）
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // 代码段：基地址 0，界限 4GB，可读可执行，Ring 0
    // Access: 0x9A = 10011010 (Present, Ring 0, Code, Executable, Readable)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // 数据段：基地址 0，界限 4GB，可读可写，Ring 0
    // Access: 0x92 = 10010010 (Present, Ring 0, Data, Writable)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    // 加载 GDT
    __asm__ volatile ("lgdt %0" : : "m"(gp));
    
    // 重新加载段寄存器
    __asm__ volatile ("ljmp $0x08, $.flush_cs\n"
                      ".flush_cs:\n"
                      "movw $0x10, %%ax\n"
                      "movw %%ax, %%ds\n"
                      "movw %%ax, %%es\n"
                      "movw %%ax, %%fs\n"
                      "movw %%ax, %%gs\n"
                      "movw %%ax, %%ss\n"
                      : : : "ax");
    
    serial_print("[INFO] GDT loaded successfully\n");
}
```

### 步骤 2.2：实现中断描述符表 (IDT)

**文件**: `kernel/idt.h`

```c
#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

// IDT 条目结构
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

// IDT 指针结构
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/**
 * 初始化 IDT
 */
void idt_init(void);

/**
 * 注册中断处理函数
 * @param num 中断号
 * @param handler 处理函数地址
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif
```

**文件**: `kernel/idt.c`

```c
#include "idt.h"
#include "io.h"

// IDT 表（256 个条目）
static struct idt_entry idt[256];
static struct idt_ptr idtp;

// 外部声明：ISR 存根函数（在 isr.asm 中定义）
extern void isr0();
extern void isr1();
extern void isr2();
// ... 根据需要添加更多 ISR

/**
 * 设置 IDT 条目
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = (base & 0xFFFF);
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

/**
 * 初始化 IDT
 */
void idt_init(void) {
    serial_print("[INFO] Initializing IDT...\n");
    
    // 设置 IDT 指针
    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base = (uint32_t)&idt;
    
    // 清空 IDT
    __builtin_memset(&idt, 0, sizeof(struct idt_entry) * 256);
    
    // 注册异常处理程序
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E); // 除零错误
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E); // 单步调试
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E); // NMI
    
    // 加载 IDT
    __asm__ volatile ("lidt %0" : : "m"(idtp));
    
    serial_print("[INFO] IDT loaded successfully\n");
}
```

### 步骤 2.3：实现中断服务例程 (ISR)

**文件**: `kernel/isr.h`

```c
#ifndef ISR_H
#define ISR_H

#include "../include/types.h"

// 中断帧结构（CPU 自动压栈）
struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed));

// ISR 处理函数类型
typedef void (*isr_handler_t)(struct registers *regs);

/**
 * 注册 ISR 处理函数
 */
void isr_register_handler(uint8_t num, isr_handler_t handler);

/**
 * 初始化 ISR
 */
void isr_init(void);

#endif
```

**文件**: `kernel/isr.c`

```c
#include "isr.h"
#include "idt.h"
#include "io.h"

// ISR 处理函数数组
static isr_handler_t isr_handlers[256];

/**
 * 注册 ISR 处理函数
 */
void isr_register_handler(uint8_t num, isr_handler_t handler) {
    isr_handlers[num] = handler;
}

/**
 * 通用 ISR 处理函数
 */
void isr_handler(struct registers *regs) {
    if (isr_handlers[regs->int_no]) {
        isr_handlers[regs->int_no](regs);
    } else {
        // 默认处理：打印异常信息
        char msg[64];
        __builtin_sprintf(msg, "[EXCEPTION] Interrupt 0x%02X\n", regs->int_no);
        serial_print(msg);
        print_color(msg, COLOR_RED);
    }
}

/**
 * 除零异常处理
 */
void divide_by_zero_handler(struct registers *regs) {
    serial_print("[ERROR] Division by zero!\n");
    print_color("FATAL: Division by zero exception!\n", COLOR_RED);
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * 初始化 ISR
 */
void isr_init(void) {
    serial_print("[INFO] Initializing ISR handlers...\n");
    
    // 注册除零异常处理
    isr_register_handler(0, divide_by_zero_handler);
    
    serial_print("[INFO] ISR handlers ready\n");
}
```

**文件**: `kernel/isr.asm`

```nasm
; ============================================================================
; 中断服务例程存根
; 每个 ISR 保存寄存器状态，调用 C 处理函数，然后恢复
; ============================================================================

global isr0
global isr1
global isr2

extern isr_handler

; 宏：定义无错误码的 ISR
%macro ISR_NOERRCODE 1
isr%1:
    push 0              ; 压入伪错误码
    push %1             ; 压入中断号
    jmp isr_common_stub
%endmacro

; 宏：定义有错误码的 ISR
%macro ISR_ERRCODE 1
isr%1:
    push %1             ; 压入中断号（错误码已在栈中）
    jmp isr_common_stub
%endmacro

; 定义前几个 ISR
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2

; 通用 ISR 处理存根
isr_common_stub:
    pusha               ; 保存所有通用寄存器
    
    ; 保存数据段选择子
    mov ax, ds
    push eax
    
    ; 加载内核数据段
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 调用 C 语言处理函数
    push esp
    call isr_handler
    add esp, 4
    
    ; 恢复数据段
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                ; 恢复所有通用寄存器
    add esp, 8          ; 清除中断号和错误码
    iret                ; 中断返回
```

### 步骤 2.4：实现可编程中断控制器 (PIC)

**文件**: `kernel/pic.h`

```c
#ifndef PIC_H
#define PIC_H

#include "../include/types.h"

/**
 * 重新映射 PIC（主从片）
 * 将 IRQ 0-15 映射到 INT 0x20-0x2F
 */
void pic_remap(void);

/**
 * 发送中断结束信号 (EOI)
 * @param irq IRQ 编号
 */
void pic_send_eoi(uint8_t irq);

/**
 * 屏蔽指定 IRQ
 * @param irq IRQ 编号
 */
void pic_mask_irq(uint8_t irq);

/**
 * 取消屏蔽指定 IRQ
 * @param irq IRQ 编号
 */
void pic_unmask_irq(uint8_t irq);

#endif
```

**文件**: `kernel/pic.c`

```c
#include "pic.h"
#include "io.h"

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define ICW1_INIT       0x10
#define ICW1_ICW4       0x01
#define ICW4_8086       0x01

/**
 * 重新映射 PIC
 */
void pic_remap(void) {
    serial_print("[INFO] Remapping PIC...\n");
    
    // 开始初始化序列 (ICW1)
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)ICW1_INIT | ICW1_ICW4), 
                      "Nd"((uint16_t)PIC1_COMMAND));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)ICW1_INIT | ICW1_ICW4), 
                      "Nd"((uint16_t)PIC2_COMMAND));
    
    // ICW2: 设置中断向量偏移量
    // 主片: IRQ 0-7 → INT 0x20-0x27
    // 从片: IRQ 8-15 → INT 0x28-0x2F
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), 
                      "Nd"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x28), 
                      "Nd"((uint16_t)PIC2_DATA));
    
    // ICW3: 告知主片从片连接在 IRQ2 上
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x04), 
                      "Nd"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x02), 
                      "Nd"((uint16_t)PIC2_DATA));
    
    // ICW4: 设置为 8086 模式
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)ICW4_8086), 
                      "Nd"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)ICW4_8086), 
                      "Nd"((uint16_t)PIC2_DATA));
    
    // 屏蔽所有中断（后续根据需要启用）
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), 
                      "Nd"((uint16_t)PIC1_DATA));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), 
                      "Nd"((uint16_t)PIC2_DATA));
    
    serial_print("[INFO] PIC remapped successfully\n");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), 
                          "Nd"((uint16_t)PIC2_COMMAND));
    }
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), 
                      "Nd"((uint16_t)PIC1_COMMAND));
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)port));
    value |= (1 << irq);
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"((uint16_t)port));
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"((uint16_t)port));
    value &= ~(1 << irq);
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"((uint16_t)port));
}
```

### 步骤 2.5：更新内核入口点

修改 `kernel/main.c`，在初始化时调用新模块：

```c
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"

void main(void) {
    // 初始化串口
    serial_init();
    serial_print("[INFO] toyOS kernel started\n");
    
    // 清屏
    clear_screen();
    print_color("================================\n", COLOR_GREEN);
    print_color("  Welcome to toyOS v0.1.0      \n", COLOR_GREEN);
    print_color("================================\n", COLOR_GREEN);
    
    // 初始化 GDT
    gdt_init();
    
    // 初始化 IDT 和 ISR
    idt_init();
    isr_init();
    
    // 重新映射 PIC
    pic_remap();
    
    // 启用中断
    __asm__ volatile ("sti");
    serial_print("[INFO] Interrupts enabled\n");
    
    // 测试除零异常（可选，注释掉以避免崩溃）
    // int x = 1 / 0;
    
    print_color("\nSystem initialized successfully.\n", COLOR_CYAN);
    print_color("Press Ctrl+A, X to exit QEMU\n", COLOR_WHITE);
    
    // 进入空闲循环
    while (1) {
        __asm__ volatile ("hlt");
    }
}
```

### 步骤 2.6：更新 Makefile

在 `Makefile` 中添加新的源文件：

```makefile
# 更新 KERNEL_SRCS
KERNEL_SRCS = kernel/main.c kernel/io.c kernel/gdt.c kernel/idt.c \
              kernel/isr.c kernel/pic.c

# 更新 KERNEL_OBJS
KERNEL_OBJS = kernel/main.o kernel/io.o kernel/gdt.o kernel/idt.o \
              kernel/isr.o kernel/pic.o

# 添加汇编编译规则
kernel/isr.o: kernel/isr.asm
	@echo "[ASM] Compiling $<"
	@$(ASM) $(ASMFLAGS) $< -o $@
```

### 步骤 2.7：测试异常处理

```bash
# 重新构建
make clean && make

# 运行
make run
```

**预期结果**:
- 串口输出显示 GDT、IDT、PIC 初始化成功
- 屏幕显示 "System initialized successfully"
- 如果取消注释 `int x = 1 / 0;`，应触发除零异常并显示红色错误信息

### ✅ 第二阶段验收检查清单

- [ ] GDT 正确加载，段寄存器更新成功
- [ ] IDT 包含至少 3 个异常处理入口
- [ ] PIC 重映射完成，IRQ 映射到 0x20-0x2F
- [ ] 触发除零异常时能捕获并打印错误信息
- [ ] 串口输出完整的初始化日志
- [ ] 系统稳定运行，无三重故障

---

## 💾 第三阶段：内存管理 (预计 10-12 小时)

### 目标
实现物理内存管理器 (PMM) 和虚拟内存管理器 (VMM)，启用分页机制

### 步骤 3.1：实现物理内存管理器 (PMM)

**文件**: `memory/pmm.h`

```c
#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PAGE_SIZE 4096

/**
 * 初始化物理内存管理器
 * @param memory_map_addr Multiboot 内存映射地址
 * @param memory_map_length 内存映射条目数
 */
void pmm_init(uint32_t memory_map_addr, uint32_t memory_map_length);

/**
 * 分配一个物理页
 * @return 物理页地址，失败返回 NULL
 */
uint32_t* pmm_alloc_page(void);

/**
 * 释放一个物理页
 * @param page 物理页地址
 */
void pmm_free_page(uint32_t* page);

/**
 * 获取可用页数
 */
uint32_t pmm_get_free_pages(void);

#endif
```

**文件**: `memory/pmm.c`

```c
#include "pmm.h"
#include "../kernel/io.h"

// 位图管理
static uint32_t *memory_map = NULL;
static uint32_t total_pages = 0;
static uint32_t free_pages = 0;

/**
 * 初始化 PMM
 */
void pmm_init(uint32_t memory_map_addr, uint32_t memory_map_length) {
    serial_print("[INFO] Initializing PMM...\n");
    
    // TODO: 解析 Multiboot 内存映射
    // 这里简化为固定 64MB 内存
    total_pages = (64 * 1024 * 1024) / PAGE_SIZE;
    free_pages = total_pages;
    
    // 分配位图空间（简化实现）
    memory_map = (uint32_t*)0x400000; // 临时硬编码地址
    
    // 清空位图
    for (uint32_t i = 0; i < total_pages / 32; i++) {
        memory_map[i] = 0;
    }
    
    char msg[64];
    __builtin_sprintf(msg, "[INFO] PMM initialized: %u pages (%u MB)\n", 
                     total_pages, total_pages * PAGE_SIZE / (1024 * 1024));
    serial_print(msg);
}

uint32_t* pmm_alloc_page(void) {
    if (free_pages == 0) {
        serial_print("[ERROR] Out of memory!\n");
        return NULL;
    }
    
    // 查找第一个空闲页
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!(memory_map[i / 32] & (1 << (i % 32)))) {
            memory_map[i / 32] |= (1 << (i % 32));
            free_pages--;
            return (uint32_t*)(i * PAGE_SIZE);
        }
    }
    
    return NULL;
}

void pmm_free_page(uint32_t* page) {
    uint32_t index = (uint32_t)page / PAGE_SIZE;
    memory_map[index / 32] &= ~(1 << (index % 32));
    free_pages++;
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}
```

### 步骤 3.2：实现虚拟内存管理器 (VMM)

**文件**: `memory/vmm.h`

```c
#ifndef VMM_H
#define VMM_H

#include "../include/types.h"

#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4

/**
 * 初始化虚拟内存管理器
 */
void vmm_init(void);

/**
 * 映射虚拟地址到物理地址
 * @param virt_addr 虚拟地址
 * @param phys_addr 物理地址
 * @param flags 页标志
 */
void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);

/**
 * 取消映射
 * @param virt_addr 虚拟地址
 */
void vmm_unmap_page(uint32_t virt_addr);

/**
 * 获取物理地址对应的虚拟地址
 */
uint32_t vmm_get_physical(uint32_t virt_addr);

#endif
```

**文件**: `memory/vmm.c`

```c
#include "vmm.h"
#include "pmm.h"
#include "../kernel/io.h"

// 页目录和页表
static uint32_t *page_directory = NULL;

/**
 * 初始化 VMM
 */
void vmm_init(void) {
    serial_print("[INFO] Initializing VMM...\n");
    
    // 分配页目录
    page_directory = (uint32_t*)pmm_alloc_page();
    __builtin_memset(page_directory, 0, PAGE_SIZE);
    
    //  identity map 前 4MB（内核空间）
    uint32_t *page_table = (uint32_t*)pmm_alloc_page();
    __builtin_memset(page_table, 0, PAGE_SIZE);
    
    page_directory[0] = (uint32_t)page_table | PAGE_PRESENT | PAGE_WRITE;
    
    for (int i = 0; i < 1024; i++) {
        page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }
    
    // 加载页目录
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory));
    
    // 启用分页
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // 设置 PG 位
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    
    serial_print("[INFO] Paging enabled\n");
}

void vmm_map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags) {
    uint32_t pd_index = (virt_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;
    
    uint32_t *page_table;
    
    // 检查页目录项是否存在
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        page_table = (uint32_t*)pmm_alloc_page();
        __builtin_memset(page_table, 0, PAGE_SIZE);
        page_directory[pd_index] = (uint32_t)page_table | PAGE_PRESENT | PAGE_WRITE;
    } else {
        page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    }
    
    page_table[pt_index] = phys_addr | flags;
}

void vmm_unmap_page(uint32_t virt_addr) {
    uint32_t pd_index = (virt_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;
    
    if (page_directory[pd_index] & PAGE_PRESENT) {
        uint32_t *page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
        page_table[pt_index] = 0;
        
        // 刷新 TLB
        __asm__ volatile ("invlpg (%0)" : : "r"(virt_addr));
    }
}

uint32_t vmm_get_physical(uint32_t virt_addr) {
    uint32_t pd_index = (virt_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;
    
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    uint32_t *page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    return page_table[pt_index] & ~0xFFF;
}
```

### 步骤 3.3：更新内核入口点

在 `kernel/main.c` 中添加内存管理初始化：

```c
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"

void main(void) {
    // ... 之前的初始化代码 ...
    
    // 初始化内存管理
    pmm_init(0, 0); // TODO: 传递真实的 Multiboot 内存映射
    vmm_init();
    
    // 测试内存分配
    uint32_t* page = pmm_alloc_page();
    if (page) {
        serial_print("[INFO] Memory allocation test passed\n");
        pmm_free_page(page);
    }
    
    print_color("\nMemory management ready.\n", COLOR_CYAN);
    
    // ... 其余代码 ...
}
```

### ✅ 第三阶段验收检查清单

- [ ] PMM 能正确分配和释放物理页
- [ ] VMM 启用分页机制
- [ ] 虚拟地址到物理地址映射正确
- [ ] 无内存泄漏或双重释放
- [ ] 系统稳定运行，无缺页异常

---

## ⌨️ 第四阶段：驱动与 Shell (预计 8-10 小时)

### 目标
实现键盘驱动和简单的命令行界面

### 步骤 4.1：实现 PS/2 键盘驱动

**文件**: `drivers/keyboard.h`

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/types.h"

/**
 * 初始化键盘驱动
 */
void keyboard_init(void);

/**
 * 读取下一个按键字符（阻塞）
 */
char keyboard_read_char(void);

#endif
```

**文件**: `drivers/keyboard.c`

```c
#include "keyboard.h"
#include "../kernel/io.h"
#include "../kernel/isr.h"

static char keyboard_buffer[256];
static int buffer_head = 0;
static int buffer_tail = 0;

// PS/2 扫描码到 ASCII 映射表
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0
};

void keyboard_handler(struct registers *regs) {
    uint8_t scancode;
    __asm__ volatile ("inb %1, %0" : "=a"(scancode) : "Nd"((uint16_t)0x60));
    
    if (scancode < 0x80 && scancode < sizeof(scancode_to_ascii)) {
        char c = scancode_to_ascii[scancode];
        if (c != 0) {
            keyboard_buffer[buffer_head] = c;
            buffer_head = (buffer_head + 1) % 256;
        }
    }
    
    // 发送 EOI
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}

void keyboard_init(void) {
    serial_print("[INFO] Initializing keyboard driver...\n");
    
    // 注册键盘中断处理程序 (IRQ1 = INT 0x21)
    isr_register_handler(0x21, keyboard_handler);
    
    // 取消屏蔽键盘 IRQ
    pic_unmask_irq(1);
    
    serial_print("[INFO] Keyboard driver ready\n");
}

char keyboard_read_char(void) {
    while (buffer_head == buffer_tail) {
        __asm__ volatile ("hlt");
    }
    
    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % 256;
    return c;
}
```

### 步骤 4.2：实现 Shell

**文件**: `shell/shell.h`

```c
#ifndef SHELL_H
#define SHELL_H

/**
 * 启动 Shell
 */
void shell_start(void);

#endif
```

**文件**: `shell/shell.c`

```c
#include "shell.h"
#include "../kernel/io.h"
#include "../drivers/keyboard.h"

#define MAX_CMD_LENGTH 256

static void print_prompt(void) {
    print_color("toyOS> ", COLOR_GREEN);
}

static void process_command(const char *cmd) {
    if (__builtin_strcmp(cmd, "help") == 0) {
        print_color("Available commands:\n", COLOR_CYAN);
        print_color("  help    - Show this help message\n", COLOR_WHITE);
        print_color("  clear   - Clear screen\n", COLOR_WHITE);
        print_color("  reboot  - Reboot system\n", COLOR_WHITE);
        print_color("  info    - Show system info\n", COLOR_WHITE);
    } else if (__builtin_strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else if (__builtin_strcmp(cmd, "reboot") == 0) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    } else if (__builtin_strcmp(cmd, "info") == 0) {
        print_color("toyOS v0.3.0\n", COLOR_CYAN);
        print_color("Architecture: x86 (32-bit)\n", COLOR_WHITE);
        print_color("Status: Running\n", COLOR_WHITE);
    } else if (cmd[0] != '\0') {
        print_color("Unknown command: ", COLOR_RED);
        print_color(cmd, COLOR_WHITE);
        print_color("\n", COLOR_WHITE);
    }
}

void shell_start(void) {
    char cmd[MAX_CMD_LENGTH];
    int cmd_index = 0;
    
    clear_screen();
    print_color("toyOS Shell v0.3.0\n", COLOR_GREEN);
    print_color("Type 'help' for available commands\n\n", COLOR_WHITE);
    
    while (1) {
        print_prompt();
        
        cmd_index = 0;
        while (1) {
            char c = keyboard_read_char();
            
            if (c == '\n') {
                cmd[cmd_index] = '\0';
                print_color("\n", COLOR_WHITE);
                break;
            } else if (c == '\b') {
                if (cmd_index > 0) {
                    cmd_index--;
                    print_color("\b \b", COLOR_WHITE);
                }
            } else if (c >= 32 && c < 127 && cmd_index < MAX_CMD_LENGTH - 1) {
                cmd[cmd_index++] = c;
                put_char(c, COLOR_WHITE);
            }
        }
        
        process_command(cmd);
    }
}
```

### 步骤 4.3：更新内核入口点

```c
#include "../shell/shell.h"

void main(void) {
    // ... 之前的初始化代码 ...
    
    // 初始化键盘
    keyboard_init();
    
    print_color("\nStarting shell...\n", COLOR_CYAN);
    
    // 启动 Shell
    shell_start();
}
```

### ✅ 第四阶段验收检查清单

- [ ] 键盘输入能被正确捕获
- [ ] Shell 提示符正常显示
- [ ] `help`、`clear`、`info` 命令工作正常
- [ ] 退格键能删除字符
- [ ] 系统可通过 `reboot` 命令重启

---

## 🐛 调试技巧汇总

### 1. 串口日志调试

```bash
# 运行 QEMU 并将串口输出到终端
make run
```

在内核中使用：
```c
serial_print("[DEBUG] Variable value: ");
char buf[32];
__builtin_sprintf(buf, "%d\n", some_variable);
serial_print(buf);
```

### 2. GDB 远程调试

```bash
# 终端 1：启动 QEMU（调试模式）
make debug

# 终端 2：启动 GDB
gdb kernel.bin
(gdb) target remote :1234
(gdb) break main
(gdb) continue
(gdb) stepi        # 单步执行汇编指令
(gdb) info registers
(gdb) x/10x 0xB8000  # 查看 VGA 缓冲区
```

### 3. QEMU Monitor

在 QEMU 运行时按 `Ctrl+A`，然后按 `C` 进入 monitor：

```
(qemu) info registers
(qemu) xp /64x 0x100000  # 查看内核代码
(qemu) stop
(qemu) cont
```

### 4. 常见问题排查

| 问题 | 可能原因 | 解决方案 |
|------|---------|---------|
| 黑屏 | 链接地址错误 | 检查 `linker.ld` 和 `objdump -h` |
| 三重故障 | IDT/GDT 配置错误 | 使用 GDB 检查段寄存器 |
| 编译错误 | 缺少依赖 | `sudo apt install gcc-multilib` |
| QEMU 立即退出 | Multiboot 头错误 | 验证 checksum 计算 |

---

## 📚 下一步扩展方向

完成四个阶段后，可以考虑以下扩展：

1. **多任务调度**：实现进程/线程管理
2. **文件系统**：支持 FAT32 或 ext2
3. **网络协议栈**：实现 TCP/IP
4. **图形界面**：VESA  framebuffer
5. **用户态程序**：ELF 加载器

---

**祝你开发顺利！🚀**

如有问题，参考 [ARCHITECTURE.md](ARCHITECTURE.md) 获取更多架构细节。
