#include "firmware.h"
#include <string.h>

/* Verified against the two ROM32A images in Ghidra. Instruction fingerprints
   include the low-battery entry point: no address from another ROM is called.
   D6600's native image/API buffers are 8c1d8000/8c1d0000; OS globals extend
   through 8c7fffff. Its RAM diagnostic (8008ca56/8008ca5c) tests ac400000
   plus 00c00000 bytes, i.e. 16 MiB SDRAM. Never reuse A8500's upper-8-MiB
   arena on D6600. These are experimental work arenas, not OS allocations. */
const struct edl_firmware edl_firmwares[] = {
    {"XD-A8500 ROM32A",
     0x8c700000,
     0x8c7f0000,
     0x8c7f8000,
     0x8c800000,
     0x80025514,
     {{0x8004634c,
       {0x4f, 0x22, 0x7f, 0xc0, 0x1f, 0x7a, 0x1f, 0x69, 0x1f, 0x58, 0xe1, 0, 0x35, 0x10, 0x8f, 2}},
      {0x802d89bc,
       {0x4f, 0x22, 0x7f, 0xfc, 0xd1, 0x96, 0x2f, 0x52, 0xd5, 0x96, 0x41, 0x0b, 0xe6, 8, 0xd7,
        0x94}},
      {0x80025514,
       {0x2f, 0xc6, 0x2f, 0xd6, 0x2f, 0xe6, 0x4f, 0x22, 0xb0, 0x15, 0xec, 1, 0x6d, 0x0d, 0xd2,
        0x4a}}}},
    {"XD-D6600 CY436 ROM32A",
     0x8c800000,
     0x8c8f0000,
     0x8c8f8000,
     0x8c900000,
     0x80024ad6,
     {{0x80047020,
       {0x4f, 0x22, 0x7f, 0xc0, 0x1f, 0x7a, 0x1f, 0x69, 0x1f, 0x58, 0xe1, 0, 0x35, 0x10, 0x8f, 2}},
      {0x802f980c,
       {0x4f, 0x22, 0x7f, 0xfc, 0xd1, 0x57, 0x2f, 0x52, 0xd5, 0x57, 0x41, 0x0b, 0xe6, 8, 0xd7,
        0x55}},
      {0x80024ad6,
       {0x2f, 0xc6, 0x2f, 0xd6, 0x2f, 0xe6, 0x4f, 0x22, 0xb0, 0x15, 0xec, 1, 0x6d, 0x0d, 0xd2,
        0x46}}}},
};
const unsigned edl_firmware_count = sizeof edl_firmwares / sizeof *edl_firmwares;

const struct edl_firmware *edl_firmware_detect(edl_rom_read read, void *ctx)
{
    if (!read)
        return NULL;
    for (unsigned i = 0; i < edl_firmware_count; i++)
    {
        unsigned j = 0;
        for (; j < 3; j++)
        {
            unsigned char bytes[16];
            const struct edl_fingerprint *f = &edl_firmwares[i].checks[j];
            if (read(f->address, bytes, sizeof bytes, ctx) || memcmp(bytes, f->bytes, sizeof bytes))
                break;
        }
        if (j == 3)
            return &edl_firmwares[i];
    }
    return NULL;
}

/* A8500 ROM32A shell syscall table (ROM 0x80376b2c) slot 0xae points to
   syscall_ae at 0x802d58c4. Selector 3 returns 0xac3c2018 and 0x100004.
   This tests the diagnostic result only; it confers NO memory ownership. */
int edl_a8500_scratch_validate(uint32_t p2_address, uint32_t bytes,
                               struct edl_scratch_range *out)
{
    if (!out || p2_address != 0xac3c2018u || bytes != 0x100004u)
        return -1;
    uint32_t physical = p2_address & 0x1fffffffu;
    uint32_t aligned = (physical + 0xffffu) & ~0xffffu;
    uint32_t remaining = physical + bytes - aligned;
    if (remaining < 0xf0000u)
        return -1;
    *out = (struct edl_scratch_range){p2_address, bytes, aligned, remaining};
    return 0;
}
