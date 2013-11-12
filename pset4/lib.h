#ifndef WEENSYOS_LIB_H
#define WEENSYOS_LIB_H
#include "types.h"

// lib.h
//
//    Functions, constants, and definitions useful in both the OS01 kernel
//    and OS01 applications.
//
//    Contents: (1) C library subset, (2) system call numbers, (3) console.


// C library subset

void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t maxlen);
char* strcpy(char* dst, const char* src);
int strcmp(const char* a, const char* b);
char* strchr(const char* s, int c);
int snprintf(char* s, size_t size, const char* format, ...);
int vsnprintf(char* s, size_t size, const char* format, va_list val);

#define RAND_MAX 0x7FFFFFFF
int rand(void);
void srand(unsigned seed);


// Assertions

// assert(x)
//    If `x == 0`, print a message and fail.
#define assert(x) \
        do { if (!(x)) assert_fail(__FILE__, __LINE__, #x); } while (0)
void assert_fail(const char* file, int line, const char* msg)
    __attribute__((noinline, noreturn));

// panic(format, ...)
//    Print the message determined by `format` and fail.
void panic(const char* format, ...) __attribute__((noinline, noreturn));


// System call numbers: an application calls `int NUM` to call a system call

#define INT_SYS                 48
#define INT_SYS_PANIC           (INT_SYS + 0)
#define INT_SYS_GETPID          (INT_SYS + 1)
#define INT_SYS_YIELD           (INT_SYS + 2)
#define INT_SYS_PAGE_ALLOC      (INT_SYS + 3)
#define INT_SYS_FORK            (INT_SYS + 4)
#define INT_SYS_EXIT            (INT_SYS + 5)


// Console printing

#define CPOS(row, col)  ((row) * 80 + (col))
#define CROW(cpos)      ((cpos) / 80)
#define CCOL(cpos)      ((cpos) % 80)

#define CONSOLE_COLUMNS 80
#define CONSOLE_ROWS    25
extern uint16_t console[CONSOLE_ROWS * CONSOLE_COLUMNS];

// current position of the cursor (80 * ROW + COL)
extern int cursorpos;

// console_clear
//    Erases the console and moves the cursor to the upper left (CPOS(0, 0)).
void console_clear(void);

// console_printf(cursor, color, format, ...)
//    Format and print a message to the x86 console.
//
//    The `format` argument supports some of the C printf function's escapes:
//    %d (to print an integer in decimal notation), %u (to print an unsigned
//    integer in decimal notation), %x (to print an unsigned integer in
//    hexadecimal notation), %c (to print a character), and %s (to print a
//    string). It also takes field widths and precisions like '%10s'.
//
//    The `cursor` argument is a cursor position, such as `CPOS(r, c)` for
//    row number `r` and column number `c`.
//
//    The `color` argument is the initial color used to print. 0x0700 is a
//    good choice (grey on black). The `format` escape %C changes the color
//    being printed.  It takes an integer from the parameter list.
//
//    Returns the final position of the cursor.
int console_printf(int cpos, int color, const char* format, ...)
    __attribute__((noinline));

// console_vprintf(cpos, color, format val)
//    The vprintf version of console_printf.
int console_vprintf(int cpos, int color, const char* format, va_list val)
    __attribute__((noinline));


// Generic print library

typedef struct printer printer;
struct printer {
    void (*putc)(printer* p, unsigned char c, int color);
};

void printer_vprintf(printer* p, int color, const char* format, va_list val);

#endif /* !WEENSYOS_LIB_H */
