#ifndef IO_H
#define IO_H

#include "../include/types.h"

#define VGA_BUFFER_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define COLOR_BLACK     0x0
#define COLOR_BLUE      0x1
#define COLOR_GREEN     0x2
#define COLOR_CYAN      0x3
#define COLOR_RED       0x4
#define COLOR_MAGENTA   0x5
#define COLOR_BROWN     0x6
#define COLOR_WHITE     0x7

#define SERIAL_COM1_BASE 0x3F8

/**
 * print string to the VGA screen
 * @param str the string to print
 */
void print_string(const char *str);

/**
 * print a string with color to the VGA screen
 * @param str the string to print
 * @param color the color to use
 */
void print_color(const char *str, uint8_t color);

/**
 * clear the VGA screen
 */
void clear_screen(void);

/** 
 * initialize the serial port
 */
void serial_init(void);

/**
 * send a char via the serial port
 * @param c the char to send
 */

void serial_putchar(char c);

/**
 * print a string via the serial port
 * @param str the string to print 
 */
void serial_print(const char *str);

#endif