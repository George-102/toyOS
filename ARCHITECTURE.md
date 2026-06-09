# toyOS 架构设计文档

## 1. 项目概述

**toyOS** 是一个从零开始构建的 32 位 x86 操作系统内核，旨在深入理解操作系统底层原理。项目运行在 QEMU 模拟器上，遵循 Multiboot 规范以实现与 GRUB 引导加载程序的兼容。

### 1.1 核心目标
- ✅ **教育性**: 清晰展示操作系统核心组件的实现原理
- ✅ **模块化**: 采用分层架构，各模块职责明确且易于替换
- ✅ **可扩展**: 为后续添加进程管理、文件系统等功能预留接口
- ✅ **可调试**: 完善的日志输出和 GDB 远程调试支持

### 1.2 技术栈
| 类别 | 工具/技术 |
|------|----------|
| **编程语言** | C (内核逻辑), Assembly (启动代码) |
| **编译器** | GCC (交叉编译器 `i686-elf-gcc`)
| **汇编器** | NASM |
| **链接器** | GNU ld (自定义链接脚本) |
| **模拟器** | QEMU (qemu-system-i386) |
| **构建工具** | Make |
| **引导规范** | Multiboot / Multiboot2 |
| **调试工具** | GDB, objdump, QEMU monitor |

---

## 2. 项目目录结构

```
toyOS/
├── ARCHITECTURE.md          # 本架构文档
├── README.md                # 项目说明和快速开始指南
├── Makefile                 # 自动化构建脚本
├── linker.ld                # 链接器脚本（定义内存布局）
│
├── boot/                    # 引导阶段代码
│   ├── boot.asm             # Multiboot 头 + 汇编入口点
│   └── multiboot.h          # Multiboot 规范常量定义
│
├── kernel/                  # 内核核心代码
│   ├── main.c               # C 语言入口点
│   ├── kernel.h             # 内核通用头文件
│   ├── io.c                 # VGA/串口 I/O 驱动
│   ├── io.h                 # I/O 接口声明
│   ├── gdt.c                # 全局描述符表实现
│   ├── gdt.h                # GDT 相关声明
│   ├── idt.c                # 中断描述符表实现
│   ├── idt.h                # IDT 相关声明
│   ├── pic.c                # 可编程中断控制器驱动
│   ├── pic.h                # PIC 接口声明
│   ├── isr.c                # 中断服务例程
│   └── isr.h                # ISR 声明
│
├── memory/                  # 内存管理模块
│   ├── pmm.c                # 物理内存管理器
│   ├── pmm.h                # PMM 接口
│   ├── vmm.c                # 虚拟内存管理器（分页）
│   └── vmm.h                # VMM 接口
│
├── drivers/                 # 硬件驱动
│   ├── keyboard.c           # PS/2 键盘驱动
│   ├── keyboard.h           # 键盘接口
│   └── timer.c              # PIT 定时器驱动
│
├── shell/                   # 命令行界面
│   ├── shell.c              # Shell 主循环
│   └── shell.h              # Shell 命令注册接口
│
├── include/                 # 公共头文件
│   ├── types.h              # 基本类型定义 (uint8_t, uint32_t 等)
│   ├── stdio.h              # 标准输入输出函数
│   └── string.h             # 字符串处理函数
│
├── utils/                   # 工具配置
│   └── grub.cfg             # GRUB 菜单配置
│
└── docs/                    # 开发文档
    ├── DEBUGGING.md         # 调试指南
    └── PHASES.md            # 开发阶段详细说明
```

---

## 3. 系统架构

### 3.1 启动流程图

```
+------------------+
|  BIOS / UEFI     |  ← QEMU 模拟固件
+--------+---------+
         |
         v
+------------------+
|  GRUB Bootloader |  ← 通过 Multiboot 规范加载内核
+--------+---------+
         |
         v
+------------------+
|  boot/boot.asm   |  ← 汇编入口: 设置堆栈、检查 Multiboot 信息
+--------+---------+
         |
         v
+------------------+
|  kernel/main.c   |  ← C 语言入口: 初始化各子系统
+--------+---------+
         |
         v
+------------------+
|  Shell / Loop    |  ← 进入命令行交互或空闲循环
+------------------+
```

### 3.2 内存布局 (linker.ld 定义)

| 地址范围 | 用途 | 说明 |
|---------|------|------|
| `0x00000000 - 0x000FFFFF` | BIOS/实模式区域 | 前 1MB，包含中断向量表等 |
| `0x00100000` | **内核加载地址** | Multiboot 规范要求内核从 1MB 开始 |
| `0x00100000 - 0x001FFFFF` | 内核代码段 (.text) | 只读，包含机器指令 |
| `0x00200000 - 0x002FFFFF` | 内核数据段 (.data/.bss) | 可读写，全局变量和静态数据 |
| **`0x00300000` 处 (BSS后)**| **内核栈底 (Stack Top)** | **初始 ESP 指向此处，向下增长 8KB，防止污染数据段** |
| `0x00301000+` | 堆内存 (Heap Area) | 供后续 `kmalloc` 分配使用 |
| `0xB8000` | VGA 文本缓冲区 | 内存映射 I/O，直接写入显示字符 |
| `0x00400000+` | 用户空间起始 | 后续用于用户进程分配 |

## 4. 核心模块职责

### 4.1 引导模块 (`boot/`)

**职责**: 
- 提供符合 Multiboot 规范的头部信息
- 建立初始执行环境（堆栈指针）
- 跳转到内核 C 入口点

**关键常量**:
```c
#define MULTIBOOT_HEADER_MAGIC  0x1BADB002
#define MULTIBOOT_HEADER_FLAGS  0x00000003
```

**工作流程**:
1. GRUB 读取 `multiboot_header`
2. 校验 checksum 后加载内核到 `0x100000`
3. 设置 `eax` 寄存器传递魔数 `0x2BADB002`
4. `ebx` 指向 Multiboot 信息结构体
5. 调用 `_start` → `main()`

---

### 4.2 输入输出模块 (`kernel/io.c`)

**VGA 文本模式驱动**:
- **目标地址**: `0xB8000` (80x25 字符模式)
- **格式**: 每个字符占 2 字节（低字节 ASCII，高字节颜色属性）
- **颜色方案**: 
  - `0x0F`: 黑底白字
  - `0x0A`: 黑底绿字（成功提示）
  - `0x0C`: 黑底红字（错误提示）

**串口调试输出** (推荐):
- **端口**: COM1 (`0x3F8`)
- **优势**: QEMU 可通过 `-serial stdio` 重定向到终端
- **用途**: 内核崩溃前的最后日志输出

**API 示例**:
```c
void print_string(const char *str);          // VGA 输出
void print_color(const char *str, uint8_t color);
void serial_putchar(char c);                 // 串口输出
void serial_print(const char *str);          // 串口字符串输出
```

---

### 4.3 全局描述符表 (`kernel/gdt.c`)

**作用**: 
在保护模式下，GDT 定义了内存段的访问权限和基地址/界限。

**多任务与用户态扩展预留**:
为了支持后续从用户态 (Ring 3) 通过中断平滑切换回内核态 (Ring 0)，GDT 必须配置并加载一个 **TSS (任务状态段)**。CPU 硬件要求在特权级切换时通过 TSS 读取内核栈指针 (`SS0` 和 `ESP0`)，否则会引发 Triple Fault。

| 段名 | 基地址 | 界限 | 类型 | 用途 |
|------|-------|------|------|------|
| NULL 段 | 0x0 | 0 | - | 捕获空指针访问 |
| 内核代码段 (Kernel Code) | 0x0 | 4GB | 0x9A (Ring 0) | 内核代码执行 |
| 内核数据段 (Kernel Data) | 0x0 | 4GB | 0x92 (Ring 0) | 内核数据读写 |
| 用户代码段 (User Code)   | 0x0 | 4GB | 0xFA (Ring 3) | 用户程序执行 |
| 用户数据段 (User Data)   | 0x0 | 4GB | 0xF2 (Ring 3) | 用户程序数据读写 |
| 任务状态段 (TSS Segment) | &tss  | sizeof(TSS) | 0x89 (Available TSS) | 保存内核栈信息 |

**数据结构**:
```c
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
```

---

### 4.4 中断描述符表 (`kernel/idt.c`)

**作用**: 
将 256 个中断向量映射到对应的中断服务程序 (ISR)。

**中断分类**:
| 范围 | 类型 | 示例 |
|------|------|------|
| 0-31 | CPU 异常 | 除零 (#DE), 缺页 (#PF) |
| 32-47 | 硬件 IRQ | 定时器 (IRQ0), 键盘 (IRQ1) |
| 48+ | 软件中断 | 系统调用 (INT 0x80) |

**IDT 条目格式**:
```c
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;        // 内核代码段选择子
    uint8_t  always0;
    uint8_t  flags;      // 0x8E = 现态中断门
    uint16_t base_hi;
} __attribute__((packed));
```

---

### 4.5 可编程中断控制器 (`kernel/pic.c`)

**作用**: 
重新映射硬件中断，避免与 CPU 异常冲突。

**默认映射问题**:
- IRQ 0-7 映射到 INT 0x08-0x0F（与 CPU 异常重叠）
- 需要重映射到 INT 0x20-0x2F

**重映射后**:
- **主 PIC**: IRQ 0-7 → INT 0x20-0x27
- **从 PIC**: IRQ 8-15 → INT 0x28-0x2F

**关键操作**:
```c
void pic_remap();        // 初始化并重新映射 PIC
void pic_send_eoi(int irq); // 发送中断结束信号
void pic_mask_irq(int irq); // 屏蔽特定 IRQ
```

---

### 4.6 内存管理 (`memory/`)

#### 物理内存管理器 (PMM)

**策略**: 位图 (Bitmap) 或 伙伴系统 (Buddy System)

**功能**:
- 跟踪 4KB 页帧的使用状态
- 提供页分配/释放接口
- 统计可用内存总量
- **内存发现**: 接收从 `boot.asm` 传来的 Multiboot 信息结构体 (`ebx` 寄存器指针)，解析其中的 `mmap` 字段，获取真实的可用物理内存页链表，避开保留的硬件和 BIOS 区域。

**API**:
```c
void pmm_init(uint32_t memory_size);
uint32_t* pmm_alloc_page();
void pmm_free_page(uint32_t* page);
```

#### 虚拟内存管理器 (VMM)

**分页机制**:
- **页大小**: 4KB
- **两级页表**: 页目录 (Page Directory) + 页表 (Page Table)
- **地址转换**: 线性地址 → 物理地址

**页目录项/页表项格式**:
```
位 31-12: 物理页框地址
位 11-0:  标志位 (Present, Read/Write, User/Supervisor 等)
```

---

## 5. 开发阶段路线图

### 📍 第一阶段：Hello World (当前阶段)

**目标**: 成功引导并在屏幕上显示 "Hello, toyOS!"

**任务清单**:
- [ ] 编写 `boot/boot.asm` 实现 Multiboot 头
- [ ] 创建 `linker.ld` 定义内存布局
- [ ] 实现 `kernel/io.c` 的 VGA 输出函数
- [ ] 编写 `Makefile` 完成自动化构建
- [ ] 生成 ISO 并通过 QEMU 测试

**验收标准**:
```bash
make run
# QEMU 窗口显示黑色背景，左上角有白色 "Hello, toyOS!" 字样
```

**预计耗时**: 2-4 小时

---

### 📍 第二阶段：基础架构完善

**目标**: 建立 GDT、IDT、PIC，能够捕获异常

**任务清单**:
- [ ] 实现 `gdt.c` 并加载 GDT
- [ ] 实现 `idt.c` 创建 IDT
- [ ] 编写常见异常的 ISR (除零、非法指令等)
- [ ] 实现 `pic.c` 重映射 IRQ
- [ ] 添加串口输出用于调试

**验收标准**:
- 触发除零异常时能打印错误信息
- GDB 可正常断点调试
- 串口能输出内核启动日志

**预计耗时**: 6-8 小时

---

### 📍 第三阶段：内存管理

**目标**: 启用分页机制，实现物理内存分配器

**任务清单**:
- [ ] 实现 PMM 位图管理
- [ ] 创建页目录和页表结构
- [ ] 启用分页（设置 CR0.PG 位）
- [ ] 实现虚拟地址映射函数
- [ ] 添加堆内存管理 (kmalloc/kfree)

**验收标准**:
- 能分配和释放物理页帧
- 虚拟地址转换正确
- 无内存泄漏

**预计耗时**: 10-12 小时

---

### 📍 第四阶段：驱动与 Shell

**目标**: 支持键盘输入和简单的命令行交互

**任务清单**:
- [ ] 实现 PS/2 键盘驱动
- [ ] 实现 PIT 定时器驱动
- [ ] 设计 Shell 命令框架
- [ ] 实现基本命令: `help`, `clear`, `reboot`, `info`
- [ ] 添加多行输入和历史记录

**验收标准**:
```
toyOS Shell v1.0
> help
Available commands: help, clear, reboot, info
> _
```

**预计耗时**: 8-10 小时

---

## 6. 构建系统说明

### Makefile 核心规则

```makefile
# 编译流程
boot.o: boot/boot.asm
    nasm -f elf32 $< -o $@

%.o: kernel/%.c
    $(CC) $(CFLAGS) -c $< -o $@

kernel.bin: boot.o kernel.o ...
    $(LD) $(LDFLAGS) -o $@ $^

os.iso: kernel.bin
    grub-mkrescue -o os.iso isodir/

run: os.iso
    qemu-i386 -cdrom os.iso -serial stdio
```

### 编译选项说明

| 选项 | 作用 |
|------|------|
| `-m32` | 生成 32 位代码 |
| `-ffreestanding` | 不依赖标准库 |
| `-nostdlib` | 不使用 libc |
| `-fno-pie` | 禁用位置无关代码 |
| `-fno-stack-protector` | 禁用栈保护（简化启动） |

---

## 7. 调试最佳实践

### 7.1 串口日志输出

**QEMU 启动参数**:
```bash
qemu-i386 -cdrom os.iso -serial stdio
```

**内核中使用**:
```c
serial_print("[INFO] Kernel started\n");
serial_print("[ERROR] Division by zero\n");
```

### 7.2 GDB 远程调试

**步骤**:
1. 启动 QEMU: `qemu-i386 -cdrom os.iso -s -S`
   - `-s`: 监听 GDB 连接 (端口 1234)
   - `-S`: 启动时暂停
2. 启动 GDB: `gdb kernel.bin`
3. 连接 QEMU: `(gdb) target remote :1234`
4. 设置断点: `(gdb) break main`
5. 继续执行: `(gdb) continue`

### 7.3 常用调试命令

```bash
# 查看汇编代码
objdump -d kernel.bin | less

# 查看符号表
nm kernel.bin

# 查看段信息
readelf -l kernel.bin

# QEMU Monitor 快捷键
Ctrl+A, C  # 切换到 QEMU monitor
info registers  # 查看寄存器状态
xp /10x 0xB8000 # 查看内存内容
```

---

## 8. 常见问题与解决方案

### Q1: QEMU 启动后黑屏
**原因**: 
- 链接地址错误
- 未正确设置 Multiboot 头

**解决**:
- 检查 `linker.ld` 中 `.text` 段是否从 `0x100000` 开始
- 使用 `objdump -h kernel.bin` 确认段布局

### Q2: 编译时报错 "undefined reference to `main`"
**原因**: 
- 链接顺序错误或文件名不匹配

**解决**:
- 确保 `boot.o` 在链接命令的最前面
- 检查 `extern main` 声明与实际函数签名一致

### Q3: GDB 断点无法命中
**原因**: 
- 优化级别过高或缺少调试信息

**解决**:
- 在 `CFLAGS` 中添加 `-g -O0`
- 使用 `add-symbol-file` 加载各个对象文件的符号

---

## 9. 扩展阅读与参考

### 官方文档
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/)
- [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html)
- [OSDev Wiki](https://wiki.osdev.org/Main_Page)

### 推荐资源
- 《Orange'S: 一个操作系统的实现》- 于渊
- 《Writing a Simple Operating System from Scratch》- Blum R.
- James Molloy 的 OS 开发教程: http://jamesmolloy.co.uk/tutorial_html/

---

## 10. 版本历史

| 版本 | 日期 | 里程碑 |
|------|------|--------|
| 0.0.1 | 2024-XX-XX | 项目初始化，完成 Hello World |
| 0.1.0 | TBD | 实现 GDT/IDT/PIC |
| 0.2.0 | TBD | 启用分页机制 |
| 0.3.0 | TBD | 键盘驱动和 Shell |

---

**维护者**: George  
**最后更新**: 2024 年
