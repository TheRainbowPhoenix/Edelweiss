#include "../exword/firmware.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int corrupted;
static int rom(uint32_t address, void *out, size_t n, void *ctx)
{
    const struct edl_firmware *f = ctx;
    for (unsigned j = 0; j < 3; j++)
        if (address == f->checks[j].address)
        {
            assert(n == 16);
            memcpy(out, f->checks[j].bytes, n);
            if (corrupted == (int)j + 1)
                ((unsigned char *)out)[15] ^= 1;
            return 0;
        }
    return -1;
}
int main(void)
{
    assert(edl_firmware_count == 2);
    for (unsigned i = 0; i < edl_firmware_count; i++)
    {
        const struct edl_firmware *f = &edl_firmwares[i];
        corrupted = 0;
        assert(edl_firmware_detect(rom, (void *)f) == f);
        for (corrupted = 1; corrupted <= 3; corrupted++)
            assert(!edl_firmware_detect(rom, (void *)f));
        assert(f->arena_end - f->arena_start == 0xf0000);
        assert(f->stack_start >= f->arena_end && f->stack_end - f->stack_start == 0x8000);
    }
    assert(!edl_firmware_detect(NULL, NULL));
    assert(edl_firmwares[1].arena_start >= 0x8c800000);
    struct edl_scratch_range scratch = {0};
    assert(!edl_a8500_scratch_validate(0xac3c2018u, 0x100004u, &scratch));
    assert(scratch.p2_address == 0xac3c2018u);
    assert(scratch.bytes == 0x100004u);
    assert(scratch.aligned_physical == 0x0c3d0000u);
    assert(scratch.aligned_bytes >= 0xf0000u);
    assert(edl_a8500_scratch_validate(0xac3c2018u, 0xf0000u, &scratch) < 0);
    assert(edl_a8500_scratch_validate(0x8c3c2018u, 0x100004u, &scratch) < 0);
    assert(edl_a8500_scratch_validate(0xac3c2018u, 0x100004u, NULL) < 0);
    puts("Firmware identification, memory bounds, scratch syscall contract passed");
}
