/* POSIX descriptors are payload-local. Native I/O always crosses the host API. */
#include "edl.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static const struct edl_api *host;
static struct
{
    int used, native, position, append;
} files[16];
void edl_gint_set_api(const struct edl_api *api)
{
    host = api;
}
static int call(unsigned id, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    if (!host)
    {
        errno = EIO;
        return -1;
    }
    int rc = host->os_call(id, a, b, c, d);
    if (rc < 0)
    {
        errno = EIO;
        return -1;
    }
    return rc;
}
static int path_native(const char *path, char out[EDL_PATH_MAX])
{
    if (!path || !host || !host->image_path)
    {
        errno = EINVAL;
        return -1;
    }
    size_t n = 0;
    if (path[0] != '/' && path[0] != '\\')
    {
        const char *last = strrchr(host->image_path, '\\');
        if (!last)
        {
            errno = EINVAL;
            return -1;
        }
        n = (size_t)(last - host->image_path) + 1;
        if (n >= EDL_PATH_MAX)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(out, host->image_path, n);
    }
    for (; *path; path++)
    {
        if (n + 1 >= EDL_PATH_MAX)
        {
            errno = ENAMETOOLONG;
            return -1;
        }
        out[n++] = *path == '/' ? '\\' : *path;
    }
    out[n] = 0;
    if (!strncmp(out, "\\\\fls0\\", 7))
        memcpy(out + 2, "drv0", 4);
    return 0;
}
static int slot(int fd)
{
    if (fd < 3 || fd >= 19 || !files[fd - 3].used)
    {
        errno = EBADF;
        return -1;
    }
    return fd - 3;
}
int open(const char *path, int flags, ...)
{
    if ((flags & O_ACCMODE) == O_RDONLY && (flags & O_TRUNC))
    {
        errno = EINVAL;
        return -1;
    }
    char native[EDL_PATH_MAX];
    if (path_native(path, native))
        return -1;
    unsigned i = 0;
    for (; i < 16 && files[i].used; i++)
        ;
    if (i == 16)
    {
        errno = EMFILE;
        return -1;
    }
    int mode = flags & O_ACCMODE;
    mode = mode == O_RDONLY ? 1 : mode == O_WRONLY ? 2 : 3;
    uint32_t p = (uint32_t)(uintptr_t)native;
    if ((flags & O_CREAT) && (flags & O_EXCL) && call(0x34, p, 1, 0, 0) < 0)
    {
        errno = EEXIST;
        return -1;
    }
    int handle = call(0x37, p, mode, 0, 0);
    if (handle < 0 && (flags & O_CREAT))
    {
        if (call(0x34, p, 1, 0, 0) < 0)
            return -1;
        handle = call(0x37, p, mode, 0, 0);
    }
    if (handle < 0)
    {
        errno = ENOENT;
        return -1;
    }
    if (flags & O_TRUNC)
    {
        call(0x38, handle, 0, 0, 0);
        if (call(0x35, p, 0, 0, 0) < 0 || call(0x34, p, 1, 0, 0) < 0)
            return -1;
        handle = call(0x37, p, mode, 0, 0);
        if (handle < 0)
            return -1;
    }
    files[i].used = 1;
    files[i].native = handle;
    files[i].position = 0;
    files[i].append = (flags & O_APPEND) != 0;
    return (int)i + 3;
}
int close(int fd)
{
    int i = slot(fd);
    if (i < 0)
        return -1;
    int rc = call(0x38, files[i].native, 0, 0, 0);
    files[i].used = 0;
    return rc;
}
off_t lseek(int fd, off_t offset, int whence)
{
    int i = slot(fd);
    if (i < 0)
        return -1;
    int base = 0;
    if (whence == SEEK_CUR)
        base = files[i].position;
    else if (whence == SEEK_END)
    {
        base = call(0x3a, files[i].native, 0, 0, 0);
        if (base < 0)
            return -1;
    }
    else if (whence != SEEK_SET)
    {
        errno = EINVAL;
        return -1;
    }
    int64_t target = (int64_t)base + offset;
    if (target < 0 || target > INT_MAX)
    {
        errno = EINVAL;
        return -1;
    }
    if (call(0x3b, files[i].native, (uint32_t)target, 0, 0) < 0)
        return -1;
    files[i].position = (int)target;
    return (off_t)target;
}
ssize_t read(int fd, void *buffer, size_t size)
{
    int i = slot(fd);
    if (i < 0)
        return -1;
    if (size > INT_MAX)
    {
        errno = EINVAL;
        return -1;
    }
    int rc = call(0x3c, files[i].native, (uint32_t)(uintptr_t)buffer, (uint32_t)size, 0);
    if (rc >= 0)
        files[i].position += rc;
    return rc;
}
ssize_t write(int fd, const void *buffer, size_t size)
{
    if (size > INT_MAX)
    {
        errno = EINVAL;
        return -1;
    }
    if (fd >= 0 && fd < 3)
        return (ssize_t)size; /* No implicit serial/console device. */
    int i = slot(fd);
    if (i < 0)
        return -1;
    if (files[i].append && lseek(fd, 0, SEEK_END) < 0)
        return -1;
    int rc = call(0x3d, files[i].native, (uint32_t)(uintptr_t)buffer, (uint32_t)size, 0);
    if (rc >= 0)
        files[i].position += rc;
    return rc;
}
int fstat(int fd, struct stat *out)
{
    int i = slot(fd);
    if (i < 0)
        return -1;
    int size = call(0x3a, files[i].native, 0, 0, 0);
    if (size < 0)
        return -1;
    memset(out, 0, sizeof *out);
    out->st_size = size;
    out->st_mode = S_IFREG | 0666;
    return 0;
}
int stat(const char *path, struct stat *out)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    int rc = fstat(fd, out), closed = close(fd);
    return rc < 0 ? rc : closed;
}
void edl_gint_close_files(void)
{
    for (int i = 0; i < 16; i++)
        if (files[i].used)
            close(i + 3);
}
