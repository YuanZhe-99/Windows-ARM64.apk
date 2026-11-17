// T08 I/O: actual Android LP64 declarations, explicit mapped paths and no libc dependency.
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

_Static_assert(sizeof(struct stat) == 128 && sizeof(struct stat64) == 128, "Android stat layout");
_Static_assert(offsetof(struct stat, st_size) == 48 && offsetof(struct stat, st_atim) == 72, "Android stat offsets");
_Static_assert(sizeof(struct timespec) == 16 && sizeof(off_t) == 8 && sizeof(ssize_t) == 8, "Android time/off/size types");

extern int t08_host_state(const int* guest_errno);
extern int t08_time_check(int64_t seconds, int64_t nanoseconds);
extern int __open_2(const char* path, int flags);
static const char* name = "/data/\xe4\xb8\xad\xe6\x96\x87 dir/\xe7\xa9\xba \xe6\xa0\xbc.bin";

/// Checks binary bytes, Unicode/space mapping, 64-bit seek, stat layout sentinels and reopening.
static int roundtrip(void) {
    errno = 181;
    int fd = open(name, O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 3 || write(fd, "A\0B\xff", 4) != 4 || lseek(fd, 1, SEEK_SET) != 1) return 1;
    char buffer[9] = {0};
    if (read(fd, buffer, 8) != 3 || memcmp(buffer, "\0B\xff", 3) || read(fd, buffer, 8)) return 2;
    const off64_t far = ((off64_t)1 << 33) + 7;
    if (lseek64(fd, far, SEEK_SET) != far || lseek(fd, 0, SEEK_CUR) != far ||
        lseek(fd, -3, SEEK_END) != 1) return 3;
    struct { uint64_t before; struct stat stat; uint64_t after; } guarded;
    memset(&guarded, 0xa5, sizeof guarded);
    struct stat64 wide;
    if (fstat(fd, &guarded.stat) || fstat64(fd, &wide) ||
        guarded.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || guarded.after != guarded.before) return 4;
    const struct stat* s = &guarded.stat;
    if (s->st_size != 4 || !S_ISREG(s->st_mode) || (s->st_mode & 0777) != 0600 ||
        s->st_blksize != 4096 || s->st_blocks < 0 || s->st_nlink < 1 || s->st_uid || s->st_gid ||
        s->st_ino != wide.st_ino || s->st_dev != wide.st_dev || wide.st_size != 4 ||
        s->st_mtim.tv_sec < 1600000000 || s->st_mtim.tv_nsec < 0 || s->st_mtim.tv_nsec >= 1000000000) return 5;
    if (read(fd, buffer, 0) || write(fd, buffer, 0) || close(fd)) return 6;
    fd = open64(name, O_RDONLY);  // No mode argument: the thunk must not interpret x2.
    if (fd < 3 || read(fd, buffer, sizeof buffer) != 4 || memcmp(buffer, "A\0B\xff", 4) || close(fd)) return 7;
    return errno == 181 && t08_host_state(&errno) ? 0 : 8;
}

/// Checks exclusive creation refusal, append independent of seek, access modes and sdcard mapping.
static int flags(void) {
    if (open(name, O_CREAT | O_EXCL | O_RDWR, 0600) != -1 || errno != EEXIST) return 1;
    int fd = open(name, O_WRONLY | O_APPEND);
    if (fd < 3 || lseek(fd, 0, SEEK_SET) || write(fd, "Z", 1) != 1) return 2;
    char buffer[8];
    if (read(fd, buffer, 1) != -1 || errno != EBADF || close(fd)) return 3;
    fd = __open_2(name, O_RDONLY);
    if (fd < 3 || write(fd, "!", 1) != -1 || errno != EBADF) return 4;
    errno = 182;
    if (read(fd, buffer, sizeof buffer) != 5 || memcmp(buffer, "A\0B\xffZ", 5) ||
        lseek(fd, 0, SEEK_END) != 5 || close(fd)) return 5;
    fd = open64("/sdcard/simple.txt", O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 3 || write(fd, "sd", 2) != 2 || close(fd)) return 6;
    return errno == 182 && t08_host_state(&errno) ? 0 : 7;
}

/// Checks Linux errno conversion and refuses unsupported paths/flags before any file mutation.
static int refusals(void) {
    if (open("/data/missing.bin", O_RDONLY) != -1 || errno != ENOENT) return 1;
    if (open(name, O_RDONLY | O_TRUNC) != -1 || errno != EINVAL ||
        open(name, O_RDWR | O_SYNC) != -1 || errno != ENOTSUP ||
        open(name, O_CREAT | O_RDWR, 0644) != -1 || errno != ENOTSUP ||
        __open_2(name, O_CREAT | O_RDWR) != -1 || errno != EINVAL) return 2;
    const char* denied[] = {"relative", "C:\\host", "/proc/self", "/data/../escape", "/data/NUL.txt",
                            "/data/ads:stream", "/data/trailing. ", "/data/escape/sentinel.txt"};
    for (size_t i = 0; i < sizeof denied / sizeof denied[0]; i++)
        if (open(denied[i], O_RDWR | O_TRUNC) != -1 || errno != EACCES) return 3;
    if (open("/data/\xff""bad", O_RDONLY) != -1 || errno != EILSEQ ||
        open("/data/\xe4\xb8\xad\xe6\x96\x87 dir", O_RDONLY) != -1 || errno != EISDIR) return 4;
    char buffer[1];
    struct stat stat;
    if (read(-1, buffer, 1) != -1 || errno != EBADF || write(-1, buffer, 1) != -1 || errno != EBADF ||
        close(-1) != -1 || errno != EBADF || fstat(-1, &stat) != -1 || errno != EBADF) return 5;
    int fd = open(name, O_RDONLY);
    if (fd < 3 || read(fd, (void*)(uintptr_t)1, 1) != -1 || errno != EFAULT ||
        lseek(fd, -1, SEEK_SET) != -1 || errno != EINVAL ||
        lseek(fd, 0, 77) != -1 || errno != EINVAL || close(fd) ||
        close(fd) != -1 || errno != EBADF) return 6;
    return t08_host_state(&errno) ? 0 : 7;
}

/// Checks epoch/units against the host, monotonic sleep deadlines and explicit time refusals.
static int clocks(void) {
    errno = 183;
    struct timespec realtime, before, after, remainder = {123, 456};
    const struct timespec pause = {0, 2000000}, zero = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &realtime) || !t08_time_check(realtime.tv_sec, realtime.tv_nsec) ||
        clock_gettime(CLOCK_MONOTONIC, &before) || nanosleep(&pause, &remainder) ||
        clock_gettime(CLOCK_MONOTONIC, &after)) return 1;
    const int64_t elapsed = (after.tv_sec - before.tv_sec) * 1000000000 + after.tv_nsec - before.tv_nsec;
    if (elapsed < 2000000 || remainder.tv_sec != 123 || remainder.tv_nsec != 456 ||
        after.tv_nsec < 0 || after.tv_nsec >= 1000000000 || nanosleep(&zero, NULL) || errno != 183) return 2;
    const struct timespec negative = {-1, 0}, invalid = {0, 1000000000}, excessive = {61, 0};
    if (nanosleep(&negative, NULL) != -1 || errno != EINVAL ||
        nanosleep(&invalid, NULL) != -1 || errno != EINVAL ||
        nanosleep(&excessive, NULL) != -1 || errno != ENOTSUP ||
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &after) != -1 || errno != ENOTSUP ||
        clock_gettime(-1, &after) != -1 || errno != EINVAL) return 3;
    return t08_host_state(&errno) ? 0 : 4;
}

/// Dispatches four bounded I/O/time cases, leaving no descriptor or managed context behind.
void* t08_case(void* argument) {
    int result;
    switch ((uintptr_t)argument) {
    case 0: result = roundtrip(); break;
    case 1: result = flags(); break;
    case 2: result = refusals(); break;
    case 3: result = clocks(); break;
    default: result = 99;
    }
    return (void*)(uintptr_t)result;
}
