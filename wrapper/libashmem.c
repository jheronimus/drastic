/* libashmem.so — Android ashmem substitute for the DraStic core on Minime.
 *
 * The Android core allocates its 64MB shared region like this:
 *   fd = open("dev/ashmem", O_RDWR | O_CREAT);
 *   if (ioctl(fd, ASHMEM_SET_SIZE, size) != 0) fd = -1;   // bails if it fails!
 *   mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
 *
 * There is no ashmem driver in the Minime kernel, so ioctl returns ENOTTY and
 * the core aborts. This preload library interposes open/openat to redirect
 * "ashmem" opens to a memfd, and interposes ioctl so ASHMEM_SET_SIZE performs
 * ftruncate on that memfd. Loaded via LD_PRELOAD ahead of the core.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

/* ASHMEM ioctls (DraStic build): SET_NAME carries a char[16] name pointer,
 * SET_SIZE carries a size_t value or pointer. Read ioctls are not needed. */
#define ASHMEM_SET_NAME    0x41007701
#define ASHMEM_SET_SIZE    0x40087703
#define ASHMEM_SET_SIZE_ALT 0x40087701

typedef int (*open_fn)(const char *, int, ...);
typedef int (*ioctl_fn)(int, int, ...);

static open_fn real_open;
static ioctl_fn real_ioctl;

static int is_ashmem_path(const char *path) {
    if (!path) return 0;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    return strcmp(base, "ashmem") == 0;
}

static int ashmem_open(void) {
    return (int)syscall(SYS_memfd_create, "ashmem", 0);
}

int open(const char *path, int flags, ...) {
    va_list ap;
    va_start(ap, flags);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);
    if (!real_open) real_open = (open_fn)dlsym(RTLD_NEXT, "open");
    if (is_ashmem_path(path)) {
        int fd = ashmem_open();
        if (fd >= 0) return fd;
        return real_open(path, flags, mode);
    }
    return real_open(path, flags, mode);
}

int openat(int dirfd, const char *path, int flags, ...) {
    va_list ap;
    va_start(ap, flags);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);
    static int (*real_openat)(int, const char *, int, ...);
    if (!real_openat) real_openat = (int (*)(int, const char *, int, ...))dlsym(RTLD_NEXT, "openat");
    if (is_ashmem_path(path) && dirfd == AT_FDCWD) {
        int fd = ashmem_open();
        if (fd >= 0) return fd;
    }
    return real_openat(dirfd, path, flags, mode);
}

int ioctl(int fd, int req, ...) {
    va_list ap;
    va_start(ap, req);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    if (!real_ioctl) real_ioctl = (ioctl_fn)dlsym(RTLD_NEXT, "ioctl");
    if (req == ASHMEM_SET_NAME) {
        return 0;
    }
    if (req == ASHMEM_SET_SIZE || req == ASHMEM_SET_SIZE_ALT) {
        size_t size = (size_t)(uintptr_t)arg;
        if (size > (1UL << 30)) size = *(size_t *)arg;
        if (ftruncate(fd, (off_t)size) == 0) return 0;
    }
    return real_ioctl(fd, req, arg);
}