#include "io.h"

static uint16_t *vga_buffer = (uint16_t *)VGA_BUFFER_ADDRESS;

static int cursor_x = 0;
static int cursor_y = 0;

static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)(0x0F)), "d"((uint16_t)0x3D4));
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)(pos & 0xFF)), "d"((uint16_t)0x3D5));
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)(0x0E)), "d"((uint16_t)0x3D4));
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)((pos >> 8) & 0xFF)), "d"((uint16_t)0x3D5));

}

static void put_char(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if(c == '\t') {
        cursor_x = (cursor_x + 8) & ~(8 -1);
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (color << 8) | c;
        cursor_x++;
    } 
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }


    if(cursor_y >= VGA_HEIGHT) {
        for(int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for(int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = (color << 8 | ' ');
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
        put_char(*str++, color);
    }
}

void clear_screen(void) {
    uint8_t color = COLOR_BLACK << 4 | COLOR_WHITE;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (color << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}


// =================serial port driver=================

void serial_init(void) {
    // 禁用中断
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)(SERIAL_COM1_BASE + 1)));
    
    // 设置波特率除数（115200 波特率）
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x80), "d"((uint16_t)(SERIAL_COM1_BASE + 3)));
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x03), "d"((uint16_t)(SERIAL_COM1_BASE + 0))); // DLL
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)(SERIAL_COM1_BASE + 1))); // DLH
    
    // 设置线路控制寄存器：8 位，无奇偶校验，1 停止位
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x03), "d"((uint16_t)(SERIAL_COM1_BASE + 3)));
    
    // 启用 FIFO
    __asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xC7), "d"((uint16_t)(SERIAL_COM1_BASE + 2)));
}

void serial_putchar(char c) {
    uint8_t status;
    
    // 等待发送缓冲区为空
    do {
        __asm__ volatile ("inb %%dx, %%al" : "=a"(status) : "d"((uint16_t)(SERIAL_COM1_BASE + 5)));
    } while ((status & 0x20) == 0);
    
    // 发送字符
    __asm__ volatile ("outb %%al, %%dx" : : "a"(c), "d"((uint16_t)SERIAL_COM1_BASE));
}

void serial_print(const char *str) {
    while (*str) {
        serial_putchar(*str);
        str++;
    }
}