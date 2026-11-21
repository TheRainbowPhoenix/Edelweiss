#include "../sdk/edl.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
extern void edl_gint_set_api(const struct edl_api *api);
extern void edl_gint_close_files(void);
static int closed, opened;
static int native(uint32_t id, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)a;
    (void)b;
    (void)d;
    if (id == 0x37)
    {
        opened++;
        return 7;
    }
    if (id == 0x38)
    {
        closed++;
        return 0;
    }
    if (id == 0x3a)
        return 100;
    if (id == 0x3b)
    {
        assert(c == 0);
        return 0;
    }
    if (id == 0x3c || id == 0x3d)
        return (int)c;
    return -1;
}
int main(void)
{
    const struct edl_api api = {.image_path = "\\\\drv0\\EDELW\\_CONTENT\\main.edl",
                                .os_call = native};
    edl_gint_set_api(&api);
    char data[8] = {0};
    struct stat st;
    int fd = open("assets/test.bin", O_RDWR);
    assert(fd == 3);
    assert(lseek(fd, 20, SEEK_SET) == 20 && read(fd, data, 5) == 5);
    assert(lseek(fd, 0, SEEK_CUR) == 25 && write(fd, data, 5) == 5);
    assert(lseek(fd, -3, SEEK_END) == 97);
    assert(!fstat(fd, &st) && st.st_size == 100);
    assert(lseek(fd, -101, SEEK_END) < 0 && errno == EINVAL);
    assert(!close(fd) && read(fd, data, 1) < 0 && errno == EBADF);
    assert(open("bad", O_RDONLY | O_TRUNC) < 0 && errno == EINVAL);
    assert(write(1, data, 3) == 3);
    fd = open("append.bin", O_WRONLY | O_APPEND);
    assert(fd == 3);
    assert(write(fd, data, 5) == 5 && lseek(fd, 0, SEEK_CUR) == 105);
    assert(!close(fd));
    for (int i = 0; i < 16; i++)
        assert(open("file", O_RDONLY) == i + 3);
    assert(open("overflow", O_RDONLY) < 0 && errno == EMFILE);
    edl_gint_close_files();
    assert(closed == opened);
    puts("Hosted POSIX I/O, descriptor limits, seek, append and cleanup checks passed");
}
