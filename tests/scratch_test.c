#include "../exword/scratch.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    struct edl_arena a = {0};
    assert(!edl_scratch_layout(0xac3c2018u, 0x100004u, &a));
    assert(a.arena_start == 0x8c3d0000u);
    assert(a.arena_end == 0x8c4b0000u);
    assert(a.stack_start == 0x8c4b8000u);
    assert(a.stack_end == 0x8c4c0000u);
    assert(a.arena_end - a.arena_start == 0xe0000u);
    assert(a.from_syscall && a.buffer_address == 0xac3c2018u);
    assert(!edl_scratch_layout(0x8c030000u, 0x80000u, &a));
    assert(a.arena_start == 0x8c030000u);
    assert(a.arena_end == 0x8c0a0000u);
    assert(a.stack_end == 0x8c0b0000u);
    assert(!edl_scratch_layout(0xac002021u, 0x60000u, &a));
    assert(a.arena_start == 0x8c010000u);
    assert(a.stack_end == 0x8c060000u);
    assert(edl_scratch_layout(0xac3c2018u, 0x100004u, NULL) < 0);
    assert(edl_scratch_layout(0x71000000u, 0x100004u, &a) < 0);
    assert(edl_scratch_layout(0xa0000000u, 0x100004u, &a) < 0);
    assert(edl_scratch_layout(0xac3c2018u, 0x20000u, &a) < 0);
    assert(edl_scratch_layout(0x8ffff000u, 0x20000u, &a) < 0);
    assert(edl_scratch_layout(0xac010000u, UINT32_MAX, &a) < 0);
    assert(edl_scratch_layout(0xac000000u, 0, &a) < 0);
    puts("Generic syscall AE arena bounds and variable 64 KiB layout passed");
    return 0;
}
