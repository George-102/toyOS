#include "io.h"

/**
 * kernel entry point
 */

 void main(void) {
    serial_init();
    serial_print("[INFO] toyOS kernel started\n");

    clear_screen();

    print_color("Welcome to toyOS!\n", COLOR_GREEN);

    print_color("=====================================\n", COLOR_GREEN);
    print_color("\nHello, toyOS!\n", COLOR_WHITE);

    serial_print("[INFO] Display output ready\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
 }