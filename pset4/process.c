#include "process.h"

// app_printf
//     A version of console_printf that picks a sensible color by process ID.

void app_printf(int colorid, const char* format, ...) {
    int color;
    if (colorid < 0)
        color = 0x0700;
    else {
        static const uint8_t col[] = { 0x0E, 0x0F, 0x0C, 0x0A, 0x09 };
        color = col[colorid % sizeof(col)] << 8;
    }

    va_list val;
    va_start(val, format);
    cursorpos = console_vprintf(cursorpos, color, format, val);
    va_end(val);
}


// panic, assert_fail
//     Call the INT_SYS_PANIC system call so the kernel loops until Control-C.

void panic(const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buf[160];
    int l = vsnprintf(buf, sizeof(buf), format, val);
    va_end(val);
    if (l > 0 && buf[l - 1] != '\n')
        strcpy(buf + l - (l == (int) sizeof(buf) - 1), "\n");
    syscall_1(INT_SYS_PANIC, (uintptr_t) buf);
 spinloop: goto spinloop;       // should never get here
}

void assert_fail(const char* file, int line, const char* msg) {
    char buf[160];
    (void) snprintf(buf, sizeof(buf), "%s:%d: assertion '%s' failed\n",
                    file, line, msg);
    syscall_1(INT_SYS_PANIC, (uintptr_t) buf);
 spinloop: goto spinloop;       // should never get here
}
