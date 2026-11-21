#include "scratch.h"
#include <stddef.h>

int edl_scratch_layout(uint32_t address, uint32_t bytes, struct edl_arena *out)
{
    if (!out || !bytes)
        return -1;
    /* Only SH7305 direct cached P1/uncached P2 SDRAM aliases. */
    if ((address < 0x8c000000u || address >= 0x90000000u) &&
        (address < 0xac000000u || address >= 0xb0000000u))
        return -1;
    uint32_t pa = address & 0x1fffffffu;
    if (bytes > 0x04000000u || pa > 0x10000000u - bytes)
        return -1;
    uint32_t aligned = (pa + EDL_SCRATCH_PAGE - 1u) & ~(EDL_SCRATCH_PAGE - 1u);
    uint32_t slack = aligned - pa;
    if (slack > bytes)
        return -1;
    /* A8500's 0x100004-byte buffer at ac3c2018 yields fifteen, not
       sixteen, complete 64 KiB frames. Never write past the returned span. */
    uint32_t usable = (bytes - slack) & ~(EDL_SCRATCH_PAGE - 1u);
    if (usable < 3u * EDL_SCRATCH_PAGE)
        return -1;
    if (usable > 0x00400000u)
        usable = 0x00400000u;
    uint32_t start = aligned | 0x80000000u;
    *out = (struct edl_arena){
        .arena_start = start,
        .arena_end = start + usable - EDL_SCRATCH_GAP - EDL_SCRATCH_STACK,
        .stack_start = start + usable - EDL_SCRATCH_STACK,
        .stack_end = start + usable,
        .buffer_address = address,
        .buffer_bytes = bytes,
        .from_syscall = 1,
    };
    return 0;
}
