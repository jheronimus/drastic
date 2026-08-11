#include "bionic_shim.h"
#include <stdio.h>
#include <stdarg.h>

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[DraStic:%s] ", tag ? tag : "Core");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    return 0;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    fprintf(stderr, "[DraStic:%s] ", tag ? tag : "Core");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    return 0;
}

int __android_log_write(int prio, const char *tag, const char *text) {
    fprintf(stderr, "[DraStic:%s] %s\n", tag ? tag : "Core", text ? text : "");
    return 0;
}

void __android_log_assert(const char *cond, const char *tag, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "[DraStic-ASSERT:%s] %s: ", tag ? tag : "Core", cond ? cond : "");
    if (fmt) {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fprintf(stderr, "\n");
    abort();
}
