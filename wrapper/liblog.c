#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>
#include <errno.h>

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio;
    va_list ap;
    fprintf(stderr, "[DraStic-Log:%s] ", tag ? tag : "Core");
    va_start(ap, fmt);
    int ret = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    return ret;
}

int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
    (void)prio;
    fprintf(stderr, "[DraStic-Log:%s] ", tag ? tag : "Core");
    int ret = vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    return ret;
}

int __android_log_write(int prio, const char *tag, const char *text) {
    (void)prio;
    int ret = fprintf(stderr, "[DraStic-Log:%s] %s\n", tag ? tag : "Core", text ? text : "");
    fflush(stderr);
    return ret;
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
    fflush(stderr);
    abort();
}

void android_set_abort_message(const char *msg) {
    fprintf(stderr, "[DraStic-ABORT] %s\n", msg ? msg : "");
    fflush(stderr);
}

alignas(8) char __sF[768];

__attribute__((constructor))
static void init_liblog(void) {
    FILE **ptrs = (FILE**)__sF;
    ptrs[0] = stdin;
    ptrs[1] = stdout;
    ptrs[2] = stderr;
}

int *__errno(void) {
    return &errno;
}
