#ifndef EDL_FIRMWARE_H
#define EDL_FIRMWARE_H
#include <stdint.h>
#include <stddef.h>
struct edl_fingerprint
{
    uint32_t address;
    unsigned char bytes[16];
};
struct edl_firmware
{
    const char *name;
    uint32_t arena_start, arena_end, stack_start, stack_end, battery;
    struct edl_fingerprint checks[3];
};
typedef int (*edl_rom_read)(uint32_t address, void *buffer, size_t size, void *ctx);
extern const struct edl_firmware edl_firmwares[];
extern const unsigned edl_firmware_count;
const struct edl_firmware *edl_firmware_detect(edl_rom_read read, void *ctx);
#endif
