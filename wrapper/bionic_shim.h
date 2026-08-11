#ifndef BIONIC_SHIM_H
#define BIONIC_SHIM_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Android Log Stubs */
#define ANDROID_LOG_UNKNOWN 0
#define ANDROID_LOG_DEFAULT 1
#define ANDROID_LOG_VERBOSE 2
#define ANDROID_LOG_DEBUG   3
#define ANDROID_LOG_INFO    4
#define ANDROID_LOG_WARN    5
#define ANDROID_LOG_ERROR   6
#define ANDROID_LOG_FATAL   7
#define ANDROID_LOG_SILENT  8

int __android_log_print(int prio, const char *tag, const char *fmt, ...);
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap);
int __android_log_write(int prio, const char *tag, const char *text);
void __android_log_assert(const char *cond, const char *tag, const char *fmt, ...);

/* Bionic Libc Stubs */
extern char __sF[768];
int *__errno(void);
void android_set_abort_message(const char *msg);

/* OpenSLES Stub Signature & Interface IDs */
typedef struct {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t  clock_seq_hi_and_reserved;
    uint8_t  clock_seq_low;
    uint8_t  node[6];
} SLInterfaceID_struct;

typedef const SLInterfaceID_struct* SLInterfaceID;

extern const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
extern const SLInterfaceID SL_IID_BUFFERQUEUE;
extern const SLInterfaceID SL_IID_ENGINE;
extern const SLInterfaceID SL_IID_PLAY;
extern const SLInterfaceID SL_IID_RECORD;
extern const SLInterfaceID SL_IID_VOLUME;

int slCreateEngine(void *pEngine, uint32_t numOptions, const void *pEngineOptions, uint32_t numInterfaces, const void *pInterfaceIds, const void *pInterfaceRequired);

/* Bionic Fortify Stubs */
int mprotect(void *addr, size_t len, int prot);
mode_t __umask_chk(mode_t mask);
char *__strcpy_chk(char *dest, const char *src, size_t dest_len);
char *__strncpy_chk2(char *dest, const char *src, size_t n, size_t dest_len, size_t src_len);
char *__strncpy_chk(char *dest, const char *src, size_t n, size_t dest_len);
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t dest_len);
void *__memmove_chk(void *dest, const void *src, size_t n, size_t dest_len);
void *__memset_chk(void *dest, int c, size_t n, size_t dest_len);
int __snprintf_chk(char *s, size_t maxlen, int flag, size_t slen, const char *format, ...);
int __vsnprintf_chk(char *s, size_t maxlen, int flag, size_t slen, const char *format, va_list ap);
int __vsprintf_chk(char *s, int flag, size_t slen, const char *format, va_list ap);
int __sprintf_chk(char *s, int flag, size_t slen, const char *format, ...);
int __fprintf_chk(FILE *fp, int flag, const char *format, ...);
int __printf_chk(int flag, const char *format, ...);
int __vfprintf_chk(FILE *fp, int flag, const char *format, va_list ap);
char *__strchr_chk(const char *s, int c, size_t s_len);
char *__strrchr_chk(const char *s, int c, size_t s_len);
size_t __strlen_chk(const char *s, size_t s_len);
void *__memchr_chk(const void *s, int c, size_t n, size_t s_len);
char *__stpcpy_chk(char *dest, const char *src, size_t dest_len);
char *__stpncpy_chk(char *dest, const char *src, size_t n, size_t dest_len);
int __open_2(const char *path, int flags);
int __open64_2(const char *path, int flags);
ssize_t __read_chk(int fd, void *buf, size_t count, size_t buf_len);
ssize_t __write_chk(int fd, const void *buf, size_t count, size_t buf_len);
ssize_t __recvfrom_chk(int fd, void *buf, size_t len, size_t buf_len, int flags, void *src_addr, void *addrlen);

#ifdef __cplusplus
}
#endif

#endif /* BIONIC_SHIM_H */
