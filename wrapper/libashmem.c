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
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
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

/* Fixed-address scratch guards.
 *
 * The core mmaps a scratch region at a fixed hint, later munmaps it and
 * re-mmaps the same hint. Under minarch's memory layout another allocation
 * (thread stack etc.) steals the hole in between, the hinted re-mmap lands
 * elsewhere, and the core's NDS init calls exit(-1). After a hinted mapping
 * is unmapped we immediately re-reserve the range PROT_NONE so nothing can
 * take it, and release the reservation only when the core asks for that
 * address again.
 */
#define GUARD_MAX 16
#define GUARD_BUDGET (512UL * 1024 * 1024)

struct guard_slot {
    void *base;
    size_t len;
    int armed;
};

static struct guard_slot g_guards[GUARD_MAX];
static pthread_mutex_t g_guard_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef void *(*mmap_fn)(void *, size_t, int, int, int, off_t);
typedef int (*munmap_fn)(void *, size_t);
static mmap_fn real_mmap;
static munmap_fn real_munmap;

static size_t guarded_bytes(void) {
    size_t total = 0;
    for (int i = 0; i < GUARD_MAX; i++) {
        if (g_guards[i].armed) total += g_guards[i].len;
    }
    return total;
}

/* Slot whose range fully covers [addr, addr+len), or -1. */
static int guard_covering(void *addr, size_t len) {
    for (int i = 0; i < GUARD_MAX; i++) {
        if (!g_guards[i].base) continue;
        char *b = (char *)g_guards[i].base;
        if (b <= (char *)addr && b + g_guards[i].len >= (char *)addr + len)
            return i;
    }
    return -1;
}

static int guard_slot_for(void *addr) {
    int free_i = -1;
    for (int i = 0; i < GUARD_MAX; i++) {
        if (g_guards[i].base == addr) return i;
        if (!g_guards[i].base && free_i < 0) free_i = i;
    }
    return free_i;
}

static void guard_drop(int i) {
    if (g_guards[i].armed) real_munmap(g_guards[i].base, g_guards[i].len);
    g_guards[i].base = NULL;
    g_guards[i].len = 0;
    g_guards[i].armed = 0;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (!real_mmap) real_mmap = (mmap_fn)dlsym(RTLD_NEXT, "mmap");

    if (addr != NULL && !(flags & MAP_FIXED)) {
        pthread_mutex_lock(&g_guard_mutex);
        int i = guard_covering(addr, length);
        if (i >= 0) {
            fprintf(stderr, "[ashmem] guard %p+%zu released for hinted mmap %p+%zu\n",
                    g_guards[i].base, g_guards[i].len, addr, length);
            fflush(stderr);
            guard_drop(i);
        }
        pthread_mutex_unlock(&g_guard_mutex);
    } else if (addr != NULL) {
        /* MAP_FIXED would silently replace our PROT_NONE pages; clear the
         * table entry so a later drop cannot unmap the core's live region. */
        pthread_mutex_lock(&g_guard_mutex);
        int i = guard_covering(addr, length);
        if (i >= 0) guard_drop(i);
        pthread_mutex_unlock(&g_guard_mutex);
    }

    void *r = real_mmap(addr, length, prot, flags, fd, offset);

    if (addr != NULL && !(flags & MAP_FIXED)) {
        if (r != addr && r != MAP_FAILED) {
            fprintf(stderr, "[ashmem] mmap hint=%p len=%zu -> %p (mismatch!)\n",
                    addr, length, r);
            fflush(stderr);
        } else if (r == addr) {
            /* Tag this hinted mapping: its later munmap gets guarded. */
            pthread_mutex_lock(&g_guard_mutex);
            int i = guard_slot_for(addr);
            if (i < 0) {
                fprintf(stderr, "[ashmem] guard table full, hint %p untracked\n", addr);
            } else {
                guard_drop(i);
                g_guards[i].base = addr;
                g_guards[i].len = length;
                g_guards[i].armed = 0;
            }
            pthread_mutex_unlock(&g_guard_mutex);
        }
    }
    return r;
}

int munmap(void *addr, size_t length) {
    if (!real_munmap) real_munmap = (munmap_fn)dlsym(RTLD_NEXT, "munmap");
    int r = real_munmap(addr, length);
    if (r != 0) {
        fprintf(stderr, "[ashmem] munmap(%p, %zu) FAILED %d\n", addr, length, r);
        fflush(stderr);
        return r;
    }
    if (addr != NULL) {
        pthread_mutex_lock(&g_guard_mutex);
        int i = guard_covering(addr, length);
        if (i >= 0 && g_guards[i].base == addr && !g_guards[i].armed) {
            if (guarded_bytes() + length > GUARD_BUDGET) {
                fprintf(stderr, "[ashmem] guard budget exhausted, %p+%zu unguarded\n",
                        addr, length);
                fflush(stderr);
            } else {
                /* Re-reserve the hole before anything else can claim it. */
                void *g = real_mmap(addr, length, PROT_NONE,
                                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
                                    -1, 0);
                if (g == addr) {
                    g_guards[i].armed = 1;
                    fprintf(stderr, "[ashmem] guard armed at %p+%zu\n", addr, length);
                } else {
                    fprintf(stderr, "[ashmem] guard FAILED at %p+%zu (got %p)\n",
                            addr, length, g);
                    g_guards[i].base = NULL;
                    g_guards[i].len = 0;
                }
                fflush(stderr);
            }
        }
        pthread_mutex_unlock(&g_guard_mutex);
    }
    return r;
}