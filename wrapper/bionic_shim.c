#include "bionic_shim.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>
#include <fcntl.h>
#include <unistd.h>

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
static void init_bionic_shim(void) {
    FILE **ptrs = (FILE**)__sF;
    ptrs[0] = stdin;
    ptrs[1] = stdout;
    ptrs[2] = stderr;
}

int *__errno(void) {
    return &errno;
}

int slCreateEngine(void *pEngine, uint32_t numOptions, const void *pEngineOptions, uint32_t numInterfaces, const void *pInterfaceIds, const void *pInterfaceRequired) {
    (void)pEngine;
    (void)numOptions;
    (void)pEngineOptions;
    (void)numInterfaces;
    (void)pInterfaceIds;
    (void)pInterfaceRequired;
    fprintf(stderr, "[DraStic-OpenSLES] slCreateEngine stub called\n");
    fflush(stderr);
    return 0; /* SL_RESULT_SUCCESS */
}

static const SLInterfaceID_struct g_SL_IID_ANDROIDSIMPLEBUFFERQUEUE = {0x19380d80, 0xf082, 0x11df, 0x8e, 0x92, {0x02, 0x42, 0xac, 0x11, 0x00, 0x02}};
static const SLInterfaceID_struct g_SL_IID_BUFFERQUEUE              = {0x05671000, 0xaa41, 0x11db, 0x9a, 0x94, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_ENGINE                   = {0x8d0865f1, 0x2ec5, 0x11db, 0x89, 0x30, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_PLAY                     = {0xef0cc080, 0x2ec5, 0x11db, 0x89, 0x30, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_RECORD                   = {0xc035a900, 0x2ec5, 0x11db, 0x89, 0x30, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};
static const SLInterfaceID_struct g_SL_IID_VOLUME                   = {0x09e8ed00, 0x2ec5, 0x11db, 0x85, 0x0a, {0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b}};

const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE = &g_SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
const SLInterfaceID SL_IID_BUFFERQUEUE              = &g_SL_IID_BUFFERQUEUE;
const SLInterfaceID SL_IID_ENGINE                   = &g_SL_IID_ENGINE;
const SLInterfaceID SL_IID_PLAY                     = &g_SL_IID_PLAY;
const SLInterfaceID SL_IID_RECORD                   = &g_SL_IID_RECORD;
const SLInterfaceID SL_IID_VOLUME                   = &g_SL_IID_VOLUME;

char *__strcpy_chk(char *dest, const char *src, size_t dest_len) {
    (void)dest_len;
    return strcpy(dest, src);
}

char *__strncpy_chk2(char *dest, const char *src, size_t n, size_t dest_len, size_t src_len) {
    (void)dest_len;
    (void)src_len;
    return strncpy(dest, src, n);
}

char *__strncpy_chk(char *dest, const char *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return strncpy(dest, src, n);
}

void *__memcpy_chk(void *dest, const void *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return memcpy(dest, src, n);
}

void *__memmove_chk(void *dest, const void *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return memmove(dest, src, n);
}

void *__memset_chk(void *dest, int c, size_t n, size_t dest_len) {
    (void)dest_len;
    return memset(dest, c, n);
}

int __snprintf_chk(char *s, size_t maxlen, int flag, size_t slen, const char *format, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(s, maxlen, format, ap);
    va_end(ap);
    return ret;
}

int __vsnprintf_chk(char *s, size_t maxlen, int flag, size_t slen, const char *format, va_list ap) {
    (void)flag;
    (void)slen;
    return vsnprintf(s, maxlen, format, ap);
}

int __vsprintf_chk(char *s, int flag, size_t slen, const char *format, va_list ap) {
    (void)flag;
    (void)slen;
    return vsprintf(s, format, ap);
}

int __sprintf_chk(char *s, int flag, size_t slen, const char *format, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(s, format, ap);
    va_end(ap);
    return ret;
}

int __fprintf_chk(FILE *fp, int flag, const char *format, ...) {
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(fp, format, ap);
    va_end(ap);
    return ret;
}

int __printf_chk(int flag, const char *format, ...) {
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

int __vfprintf_chk(FILE *fp, int flag, const char *format, va_list ap) {
    (void)flag;
    return vfprintf(fp, format, ap);
}

char *__strchr_chk(const char *s, int c, size_t s_len) {
    (void)s_len;
    return strchr(s, c);
}

char *__strrchr_chk(const char *s, int c, size_t s_len) {
    (void)s_len;
    return strrchr(s, c);
}

size_t __strlen_chk(const char *s, size_t s_len) {
    (void)s_len;
    return strlen(s);
}

void *__memchr_chk(const void *s, int c, size_t n, size_t s_len) {
    (void)s_len;
    return memchr(s, c, n);
}

char *__stpcpy_chk(char *dest, const char *src, size_t dest_len) {
    (void)dest_len;
    return stpcpy(dest, src);
}

char *__stpncpy_chk(char *dest, const char *src, size_t n, size_t dest_len) {
    (void)dest_len;
    return stpncpy(dest, src, n);
}

int __open_2(const char *path, int flags) {
    int res = open(path, flags);
    fprintf(stderr, "[Bionic-Shim] __open_2('%s', 0x%x) -> %d\n", path ? path : "", flags, res);
    fflush(stderr);
    return res;
}

int __open64_2(const char *path, int flags) {
    int res = open(path, flags);
    fprintf(stderr, "[Bionic-Shim] __open64_2('%s', 0x%x) -> %d\n", path ? path : "", flags, res);
    fflush(stderr);
    return res;
}

ssize_t __read_chk(int fd, void *buf, size_t count, size_t buf_len) {
    (void)buf_len;
    ssize_t res = read(fd, buf, count);
    fprintf(stderr, "[Bionic-Shim] __read_chk(fd=%d, count=%zu) -> %zd\n", fd, count, res);
    fflush(stderr);
    return res;
}

ssize_t __write_chk(int fd, const void *buf, size_t count, size_t buf_len) {
    (void)buf_len;
    ssize_t res = write(fd, buf, count);
    fprintf(stderr, "[Bionic-Shim] __write_chk(fd=%d, count=%zu) -> %zd\n", fd, count, res);
    fflush(stderr);
    return res;
}

ssize_t __recvfrom_chk(int fd, void *buf, size_t len, size_t buf_len, int flags, void *src_addr, void *addrlen) {
    (void)buf_len;
    return recvfrom(fd, buf, len, flags, (struct sockaddr*)src_addr, (socklen_t*)addrlen);
}

#include <sys/mman.h>
#include <dlfcn.h>

int mprotect(void *addr, size_t len, int prot) {
    typedef int (*pfn_mprotect)(void *, size_t, int);
    static pfn_mprotect real_mprotect = NULL;
    if (!real_mprotect) {
        real_mprotect = (pfn_mprotect)dlsym(RTLD_NEXT, "mprotect");
    }
    int res = real_mprotect ? real_mprotect(addr, len, prot) : 0;
    fprintf(stderr, "[Bionic-Shim] mprotect(%p, %zu, 0x%x) -> %d\n", addr, len, prot, res);
    fflush(stderr);
    if (res != 0) {
        fprintf(stderr, "[Bionic-Shim] WARNING: real mprotect failed (errno=%d), returning success stub for DraStic JIT\n", errno);
        fflush(stderr);
        return 0;
    }
    return res;
}

mode_t __umask_chk(mode_t mask) {
    return umask(mask);
}
