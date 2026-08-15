/* libandroid.so — minimal Android NDK library shim for the DraStic core.
 *
 * The core dlopens "libandroid.so" at runtime and resolves
 * ASharedMemory_create (API 26+). Minime has no libandroid.so, so we provide
 * one backed by memfd_create. Any other libandroid symbol the core may try to
 * resolve lazily is intentionally absent (returns NULL via dlsym).
 */
#define _GNU_SOURCE
#include <stddef.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

int ASharedMemory_create(const char *name, size_t size) {
    int fd = (int)syscall(SYS_memfd_create, name ? name : "ASharedMemory", 0);
    if (fd >= 0 && size > 0 && ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}