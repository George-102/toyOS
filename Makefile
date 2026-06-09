CC = gcc 
LD = ld
ASM = nasm

CFLAGS = -m32 -ffreestanding -nostdlib -no-pie -no-pie -fno-stack-protector -g -O0 -Wall -Wextra
LDFLAGS = -m elf_i386 -T linker.ld
ASMFLAGS = -f elf32

#Target files
TARGET = os.iso
KERNEL_BIN = kernel.bin

# Source files
BOOT_SRC = boot/boot.asm
KERNEL_SRCS = kernel/main.c kernel/io.c

# Object files
BOOT_OBJ = boot/boot.o
KERNEL_OBJS = kernel/main.o kernel/io.o

all: $(TARGET) 

$(BOOT_OBJ): $(BOOT_SRC)
	@echo "[ASM] Compiling $<"
	@$(ASM) $(ASMFLAGS) $< -o $@

kernel/%.o: kernel/%.c
	@echo "[CC] Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJS)
	@echo "[LD] Linking $@"
	@$(LD) $(LDFLAGS) -o $@ $^

$(TARGET): $(KERNEL_BIN)
	@echo "[ISO] Creating bootable ISO..."
	@mkdir -p isodir/boot/grub
	@cp $(KERNEL_BIN) isodir/boot/kernel.bin
	@echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	@echo 'menuentry "toyOS" {' >> isodir/boot/grub/grub.cfg
	@echo '	    multiboot /boot/kernel.bin' >> isodir/boot/grub/grub.cfg
	@echo '     boot' >> isodir/boot/grub/grub.cfg
	@echo '}' >> isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(TARGET) isodir 2>/dev/null
	@rm -rf isodir
	@echo "[DONE] ISO created: $(TARGET)"

run: $(TARGET)
	@echo "[QEMU] Starting toyOS..."
	@qemu-i386 -kernel $(KERNEL_BIN) -serial stdio -vga std

debug: $(TARGET)
	@echo "[QEMU] Starting toyOS in debug mode..."
	@echo "Connect with: gdb kernel.bin -> target remote :1234"
	@qemu-i386 -kernel $(KERNEL_BIN) -serial stdio -vga std -s -S

clean: 
	@echo "[CLEAN] Cleaning..."
	@rm -f *.o kernel/*.o $(KERNEL_BIN) $(TARGET)
	@rm -rf isodir
	@echo "[CLEAN] Done."

disasm: $(KERNEL_BIN)
	@objdump -d $(KERNEL_BIN) | less

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