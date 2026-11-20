#include "host.h"
#include <string.h>

/* ROM32A.BIN/XD-A8500. Different packaging languages do not select a
   different firmware memory profile. Unknown firmware fails before RAM writes.
   The upper 1 MiB is an experimental arena, based on the 8 MiB SDRAM save
   boundary and the ROM's observed work buffers below it. Hardware validation
   is required; it is not an OS allocator reservation. */
int edl_profile_supported(void)
{
    static const unsigned char loader[16]={
        0x4f,0x22,0x7f,0xc0,0x1f,0x7a,0x1f,0x69,0x1f,0x58,0xe1,0,0x35,0x10,0x8f,2};
    static const unsigned char invoke[16]={
        0xd0,4,0x61,2,0xd0,4,0x60,2,0x20,0x12,0xd0,4,0x60,2,0x40,0x2b};
    static const unsigned char content[16]={
        0x4f,0x22,0x7f,0xfc,0xd1,0x96,0x2f,0x52,0xd5,0x96,0x41,0x0b,0xe6,8,0xd7,0x94};
    return !memcmp((const void *)0x8004634c,loader,sizeof loader) &&
        !memcmp((const void *)0x80303960,invoke,sizeof invoke) &&
        !memcmp((const void *)0x802d89bc,content,sizeof content);
}

void edl_sync_code(void)
{
    /* Payload bytes were written through uncached P2 aliases. Discard cached
       instructions before using the corresponding P1 executable addresses. */
    __asm__ volatile("synco" ::: "memory");
    for(uint32_t p=EDL_ARENA_START;p<EDL_ARENA_END;p+=32)
        __asm__ volatile("icbi @%0" :: "r"(p) : "memory");
    __asm__ volatile("synco" ::: "memory");
}
