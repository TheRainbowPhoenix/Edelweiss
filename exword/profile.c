#include "host.h"
#include <string.h>
#include <gint/gint.h>
#include <gint/cpu.h>

const struct edl_firmware *edl_current;
static int rom_read(uint32_t address, void *out, size_t size, void *ctx)
{
    (void)ctx;
    memcpy(out, (const void *)(uintptr_t)address, size);
    return 0;
}
int edl_profile_init(void)
{
    edl_current = edl_firmware_detect(rom_read, NULL);
    if (!edl_current)
        return -10;
    if (edl_current->arena_start < 0x8c800000)
        return 0;
    /* D6600: reject 8-MiB aliases before touching the extended arena. Each
       probe restores both words with interrupts masked, using uncached P2.
       Flush lower aliases first so a later dirty eviction cannot undo this. */
    cpu_atomic_start();
    int rc = 0;
    for (uint32_t p = edl_current->arena_start; p < edl_current->stack_end; p += 4096)
    {
        uint32_t low = p - 0x800000;
        __asm__ volatile("ocbp @%0" ::"r"(low) : "memory");
        __asm__ volatile("ocbp @%0" ::"r"(p) : "memory");
        __asm__ volatile("synco" ::: "memory");
        volatile uint32_t *a = (void *)(uintptr_t)(p | 0x20000000);
        volatile uint32_t *b = (void *)(uintptr_t)(low | 0x20000000);
        uint32_t av = *a, bv = *b, pattern = ~bv;
        *a = pattern;
        __asm__ volatile("synco" ::: "memory");
        if (*a != pattern || *b != bv)
            rc = -14;
        *a = av;
        *b = bv;
        __asm__ volatile("synco" ::: "memory");
        if (rc)
            break;
    }
    cpu_atomic_end();
    if (rc)
        return rc;
    for (uint32_t p = edl_current->arena_start; p < edl_current->stack_end; p += 32)
        __asm__ volatile("ocbp @%0" ::"r"(p) : "memory");
    __asm__ volatile("synco" ::: "memory");
    /* The supplied D6600 RAM dump has an empty upper 8 MiB. Refuse a ROM
       variant that is already using this candidate area. This is a guard,
       not an allocator reservation or a claim of support for unknown ROMs. */
    for (uint32_t p = edl_current->arena_start; p < edl_current->stack_end; p += 4)
        if (*(volatile uint32_t *)(uintptr_t)(p | 0x20000000))
            return -13;
    return 0;
}
void edl_profile_release(void)
{
    if (!edl_current || edl_current->arena_start < 0x8c800000)
        return;
    for (uint32_t p = edl_current->arena_start; p < edl_current->stack_end; p += 32)
        __asm__ volatile("ocbp @%0" ::"r"(p) : "memory");
    __asm__ volatile("synco" ::: "memory");
    memset((void *)(uintptr_t)(edl_current->arena_start | 0x20000000), 0,
           edl_current->stack_end - edl_current->arena_start);
    __asm__ volatile("synco" ::: "memory");
}
void edl_sync_code(void)
{
    __asm__ volatile("synco" ::: "memory");
    for (uint32_t p = edl_current->arena_start; p < edl_current->arena_end; p += 32)
        __asm__ volatile("icbi @%0" ::"r"(p) : "memory");
    __asm__ volatile("synco" ::: "memory");
}
static int battery_native(void)
{
    int (*above_low)(void) = (void *)(uintptr_t)edl_current->battery;
    int rc = above_low();
    return rc == 0 || rc == 1 ? rc : -1;
}
int edl_battery_state(void)
{
    return edl_current ? gint_world_switch(GINT_CALL(battery_native)) : -1;
}
