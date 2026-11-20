#ifndef EDELWEISS_SDK_H
#define EDELWEISS_SDK_H
#include <stdint.h>
#include <stddef.h>

#define EDL_ABI_MAGIC 0x45444c31u
#define EDL_ABI_VERSION 1
#define EDL_PATH_MAX 256
#define EDL_TITLE_MAX 32
#define EDL_ICON_SIZE 32
#define EDL_EXIT 0
#define EDL_LAUNCH 1

enum edl_key { EDL_UP=1, EDL_DOWN, EDL_LEFT, EDL_RIGHT, EDL_OK,
    EDL_BACK, EDL_REFRESH, EDL_TOUCH, EDL_HOME };
struct edl_event { int key, x, y; };
struct edl_status {
    uint16_t year;
    uint8_t month, day, hours, minutes, seconds; /* month is 1..12 */
    int8_t battery; /* -1 unknown, 0 low, 1 normal; not a percentage */
};
struct edl_record { char path[EDL_PATH_MAX]; char title[EDL_TITLE_MAX]; };
/* Serialized big-endian in a non-allocated .edl.meta ELF section. */
struct edl_metadata {
    uint32_t magic, version;
    char title[32], description[64];
    uint16_t icon[32*32];
};

/* An EDL runs in the resident gint world. All pointers live for the current
   invocation only. OS APIs MUST run through os_call or the provided services.
   image_path is the full native path, not a drive-relative name. */
struct edl_api {
    uint32_t magic, version, size;
    uint32_t width, height;
    uintptr_t syscall_dispatcher;
    const char *image_path;
    void (*rect)(int x, int y, int w, int h, uint16_t color);
    void (*text)(int x, int y, uint16_t color, const char *text);
    void (*blit)(int x, int y, int w, int h, const uint16_t *pixels);
    void (*present)(void);
    int (*wait)(struct edl_event *event);
    /* Return total count; copy [first,first+capacity) into rows. Negative:
       native FS error. Missing card mounts are skipped. */
    int (*scan)(unsigned first, struct edl_record *rows, unsigned capacity);
    int (*metadata)(const char *path, struct edl_metadata *out);
    int (*read)(const char *path, uint32_t offset, void *dst, uint32_t size);
    int (*os_call)(uint32_t id, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
    /* Copy a requested next path into resident storage. Return EDL_LAUNCH
       from edl_main after success. Never launch recursively: payloads share
       a backing arena. The old payload is fully unwound before replacement. */
    int (*launch)(const char *path);
    uint32_t arena_start, arena_end, stack_start, stack_end;
    /* Append-only ABI extension; the original v1 prefix remains compatible. */
    int (*status)(struct edl_status *out);
    /* timeout_ms=0 waits indefinitely. Expiration returns key=0, not a keypress. */
    int (*wait_timeout)(struct edl_event *event, unsigned timeout_ms);
};

int edl_main(const struct edl_api *api);
#define EDL_METADATA __attribute__((used,section(".edl.meta"),aligned(4)))
#endif
