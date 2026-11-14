/**
 * The newlib system calls that stdio links in but this firmware never uses (printf only writes; _write is in
 * hc32_ll_utility.c). Same answers as libnosys (-1 / 0 with ENOSYS), defined here so the link has no
 * "not implemented and will always fail" warnings.
 */
#include <errno.h>
#include <sys/stat.h>

#undef errno
extern int errno;

int _close(int fd) { (void)fd; errno = ENOSYS; return -1; }
int _fstat(int fd, struct stat* st) { (void)fd; (void)st; errno = ENOSYS; return -1; }
int _isatty(int fd) { (void)fd; errno = ENOSYS; return 0; }
int _lseek(int fd, int off, int whence) { (void)fd; (void)off; (void)whence; errno = ENOSYS; return -1; }
int _read(int fd, char* buf, int len) { (void)fd; (void)buf; (void)len; errno = ENOSYS; return -1; }
