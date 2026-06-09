# toyOS - 32位 x86 操作系统

<div align="center">

![Platform](https://img.shields.io/badge/platform-x86--32-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Status](https://img.shields.io/badge/status-developing-yellow)
![Language](https://img.shields.io/badge/language-C%20%7C%20Assembly-orange)

**一个从零开始构建的教育性操作系统内核**

[架构设计](ARCHITECTURE.md) · [开发指南](docs/DEVELOPMENT_GUIDE.md) · [报告问题](https://github.com/yourusername/toyOS/issues)

</div>

---

## 📖 项目简介

**toyOS** 是一个遵循 Multiboot 规范的 32 位 x86 操作系统内核，旨在帮助开发者深入理解操作系统的底层工作原理。项目采用模块化设计，从引导加载到内存管理、中断处理、设备驱动等核心功能逐步实现。

### ✨ 核心特性

- 🎯 **教育导向**: 代码清晰注释，每个模块都有详细的文档说明
- 🧩 **模块化架构**: Boot、Kernel、Memory、Drivers 分层设计，易于理解和扩展
- 🔧 **完善的调试支持**: VGA 文本输出 + 串口日志 + GDB 远程调试
- 📚 **分阶段开发**: 4个明确的开发阶段，每阶段都有可运行的里程碑
- 🌐 **标准兼容**: 遵循 Multiboot 规范，可通过 GRUB 引导加载

---

## 🛠️ 技术栈

| 类别 | 技术/工具 |
|------|----------|
| **编程语言** | C (内核逻辑), NASM Assembly (引导代码) |
| **编译器** | GCC (`i686-elf-gcc` 或 `gcc -m32`) |
| **汇编器** | NASM |
| **链接器** | GNU ld (自定义链接脚本) |
| **模拟器** | QEMU (qemu-system-i386) |
| **构建工具** | Make |
| **引导规范** | Multiboot / Multiboot2 |
| **调试工具** | GDB, objdump, QEMU monitor |

---

## 🚀 快速开始

### 环境要求

- **操作系统**: Linux (推荐 Ubuntu/WSL) 或 macOS
- **必需工具**:
  ```bash
  sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin grub-common xorriso gdb
  ```

### 构建项目

```bash
# 克隆仓库
git clone https://github.com/yourusername/toyOS.git
cd toyOS

# 编译所有文件
make all

# 或者只编译内核二进制文件
make kernel.bin
```

### 运行系统

```bash
# 方式 1: 直接运行内核（推荐用于开发）
make run

# 方式 2: 通过 ISO 镜像运行（模拟真实启动）
make run-iso

# 方式 3: 开启 GDB 远程调试
make debug
```

### 清理构建产物

```bash
make clean
```

---

## 📁 项目结构

```
toyOS/
├── 📄 README.md                  # 项目说明（本文件）
├── 📄 ARCHITECTURE.md            # 详细架构设计文档
├── 📄 Makefile                   # 自动化构建脚本
├── 📄 linker.ld                  # 链接器脚本（内存布局定义）
│
├── 📂 boot/                      # 引导阶段代码
│   └── boot.asm                  # Multiboot 头 + 汇编入口点
│
├── 📂 kernel/                    # 内核核心代码
│   ├── main.c                    # C 语言入口点
│   ├── io.c / io.h               # VGA/串口 I/O 驱动
│   ├── gdt.c / gdt.h             # 全局描述符表
│   ├── idt.c / idt.h             # 中断描述符表
│   └── pic.c / pic.h             # 可编程中断控制器
│
├── 📂 include/                   # 公共头文件
│   └── types.h                   # 基本类型定义
│
├── 📂 memory/                    # 内存管理模块（待实现）
│   ├── pmm.c / pmm.h             # 物理内存管理器
│   └── vmm.c / vmm.h             # 虚拟内存管理器
│
├── 📂 drivers/                   # 硬件驱动（待实现）
│   ├── keyboard.c / keyboard.h   # PS/2 键盘驱动
│   └── timer.c / timer.h         # PIT 定时器
│
├── 📂 shell/                     # 命令行界面（待实现）
│   ├── shell.c / shell.h         # Shell 主循环
│
├── 📂 utils/                     # 工具配置
│   └── grub.cfg                  # GRUB 菜单配置
│
└── 📂 docs/                      # 开发文档
    └── DEVELOPMENT_GUIDE.md      # 分阶段开发指南
```

---

## 🗺️ 开发路线图

toyOS 采用分阶段开发策略，每个阶段都有明确的目标和可验证的里程碑：

### 📍 第一阶段：Hello World ✅ (当前阶段)
- [x] Multiboot 引导头实现
- [x] VGA 文本模式输出
- [x] 串口调试支持
- [x] 自动化构建系统

**验收标准**: QEMU 窗口显示 "Hello, toyOS!"

### 📍 第二阶段：基础架构 🚧 (进行中)
- [ ] GDT (全局描述符表) 实现
- [ ] IDT (中断描述符表) 实现
- [ ] PIC (中断控制器) 重映射
- [ ] CPU 异常处理

**预计耗时**: 6-8 小时

### 📍 第三阶段：内存管理 ⏳ (计划中)
- [ ] 物理内存管理器 (PMM)
- [ ] 分页机制启用
- [ ] 虚拟内存管理器 (VMM)
- [ ] 堆内存分配 (kmalloc/kfree)

**预计耗时**: 10-12 小时

### 📍 第四阶段：驱动与 Shell ⏳ (计划中)
- [ ] PS/2 键盘驱动
- [ ] PIT 定时器驱动
- [ ] Shell 命令框架
- [ ] 基本命令实现 (help, clear, reboot, info)

**预计耗时**: 8-10 小时

详细开发步骤请参考 [开发指南](docs/DEVELOPMENT_GUIDE.md)。

---

## 🔍 调试方法

### 串口日志输出（推荐）

```bash
# 启动时重定向串口到终端
qemu-i386 -kernel kernel.bin -serial stdio -vga std
```

在内核中使用：
```c
serial_print("[INFO] Kernel started\n");
serial_print("[ERROR] Division by zero at 0x12345\n");
```

### GDB 远程调试

```bash
# 终端 1: 启动 QEMU（暂停在入口处）
make debug

# 终端 2: 启动 GDB
gdb kernel.bin
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

### 查看汇编代码

```bash
# 反汇编内核
objdump -d kernel.bin | less

# 查看符号表
nm kernel.bin

# 查看段信息
readelf -l kernel.bin
```

---

## ❓ 常见问题

### Q1: QEMU 启动后黑屏怎么办？

**可能原因**:
- 链接地址错误
- Multiboot 头校验失败

**解决方案**:
```bash
# 检查段布局
objdump -h kernel.bin

# 确认 .text 段从 0x100000 开始
# 检查 multiboot_header 是否在前 8KB 内
```

### Q2: 编译时报错 "undefined reference to `main`"

**解决方案**:
- 确保 `boot.o` 在链接命令的最前面
- 检查 `extern main` 声明与实际函数签名一致

### Q3: WSL 中 QEMU 无法显示图形界面？

**解决方案**:
使用 `-kernel` 参数代替 `-cdrom`:
```bash
qemu-i386 -kernel kernel.bin -serial stdio -vga std
```

更多问题请参考 [架构文档](ARCHITECTURE.md) 第 8 节。

---

## 🤝 贡献指南

欢迎贡献代码、报告问题或提出改进建议！

1. **Fork** 本仓库
2. 创建特性分支: `git checkout -b feature/amazing-feature`
3. 提交更改: `git commit -m 'Add some amazing feature'`
4. 推送到分支: `git push origin feature/amazing-feature`
5. 提交 **Pull Request**

### 代码规范

- C 代码遵循 Linux 内核编码风格
- 汇编代码使用清晰的注释和标签
- 每个函数必须有文档注释
- 提交前运行 `make clean && make all` 确保无编译错误


## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

---

## 👤 作者

**George** 

- GitHub: [@Christopher](https://github.com/George-102)

---

## 🙏 致谢

感谢以下开源项目和社区的贡献：
- [OSDev.org](https://wiki.osdev.org/) - 操作系统开发社区
- [QEMU](https://www.qemu.org/) - 强大的硬件模拟器
- [GRUB](https://www.gnu.org/software/grub/) - 通用引导加载程序

---

<div align="center">

**如果这个项目对你有帮助，请给个 ⭐ Star 支持一下！**

Made with ❤️ by Christopher

</div>
