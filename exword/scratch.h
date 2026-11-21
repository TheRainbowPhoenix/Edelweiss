#ifndef EDL_SCRATCH_H
#define EDL_SCRATCH_H
#include <stdint.h>

#define EDL_SCRATCH_PAGE 0x10000u
#define EDL_SCRATCH_STACK 0x8000u
#define EDL_SCRATCH_GAP 0x8000u

/* Native syscall 0xae selector 3 returns a host-owned buffer. Its lifetime
   and exclusivity are device compatibility assumptions, not malloc/free. */
struct edl_arena
{
    uint32_t arena_start, arena_end, stack_start, stack_end;
    uint32_t buffer_address, buffer_bytes;
    int from_syscall;
};

/* Derive a runtime P1 arena from complete 64 KiB SDRAM chunks, reserving
   the final chunk for a 32 KiB guard gap and 32 KiB stack. */
int edl_scratch_layout(uint32_t address, uint32_t bytes, struct edl_arena *out);
#endif
