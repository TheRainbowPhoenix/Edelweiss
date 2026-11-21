#include "memory.h"
#include <string.h>

static int owned(const struct edl_memory *memory)
{
    return memory && memory->allocation && !memory->poisoned;
}

int edl_memory_physical(void *allocation, size_t bytes, uint32_t *physical)
{
    uintptr_t raw = (uintptr_t)allocation;
    if (!physical || raw < 0x8c000000u || raw >= 0x90000000u ||
        bytes < EDL_MEMORY_BYTES + EDL_MEMORY_PAGE - 1u || bytes > UINT32_MAX ||
        raw > UINT32_MAX - bytes)
        return -1;
    uint32_t aligned = ((uint32_t)raw + EDL_MEMORY_PAGE - 1u) & ~(EDL_MEMORY_PAGE - 1u);
    if (aligned < raw || aligned - raw > bytes || EDL_MEMORY_BYTES > bytes - (aligned - raw) ||
        aligned > 0x90000000u - EDL_MEMORY_BYTES)
        return -1;
    *physical = aligned & 0x1fffffffu;
    return 0;
}

int edl_memory_open(struct edl_memory *memory, const struct edl_memory_ops *ops)
{
    if (!memory || !ops || !ops->alloc || !ops->free || !ops->map || !ops->unmap)
        return -1;
    memset(memory, 0, sizeof *memory);
    memory->ops = *ops;
    const size_t bytes = EDL_MEMORY_BYTES + EDL_MEMORY_PAGE - 1u;
    void *p = ops->alloc(ops->context, bytes);
    if (!p)
        return -1;
    uint32_t physical;
    if (edl_memory_physical(p, bytes, &physical))
    {
        ops->free(ops->context, p);
        return -1;
    }
    memory->allocation = p;
    memory->physical = physical;
#if defined(__sh__)
    /* Evict any dirty cached aliases before the ELF writer uses P2. */
    for (uint32_t offset = 0; offset < EDL_MEMORY_BYTES; offset += 32)
    {
        uint32_t address = (physical + offset) | 0x80000000u;
        __asm__ volatile("ocbp @%0" ::"r"(address) : "memory");
    }
    __asm__ volatile("synco" ::: "memory");
#endif
    return 0;
}

int edl_memory_resolve(const struct edl_memory *memory, uint32_t va, size_t size,
                       uint32_t *physical)
{
    if (!owned(memory) || !physical || va < EDL_MEMORY_VBASE ||
        va >= EDL_MEMORY_VBASE + EDL_MEMORY_BYTES)
        return -1;
    uint32_t offset = va - EDL_MEMORY_VBASE;
    if (size > EDL_MEMORY_BYTES - offset)
        return -1;
    *physical = memory->physical + offset;
    return 0;
}

int edl_memory_write(void *memory, uint32_t va, const void *data, size_t size)
{
    struct edl_memory *owned_memory = memory;
    uint32_t physical;
    if (!owned(owned_memory) || owned_memory->mapped || owned_memory->loaded || !data ||
        edl_memory_resolve(memory, va, size, &physical))
        return -1;
#if defined(__sh__)
    /* The uncached P2 alias is safe for loader writes while the P0 window
       is unmapped. The owner flushes dirty P1 aliases before writing. */
    memcpy((void *)(uintptr_t)(physical | 0xa0000000u), data, size);
    return 0;
#else
    /* Never dereference fabricated SH P2 addresses in host unit tests. */
    (void)physical;
    return -1;
#endif
}

int edl_memory_finish_load(struct edl_memory *memory)
{
    if (!owned(memory) || memory->mapped || memory->loaded)
        return -1;
#if defined(__sh__)
    __asm__ volatile("synco" ::: "memory");
    for (uint32_t offset = 0; offset < EDL_MEMORY_BYTES; offset += 32)
    {
        uint32_t address = (memory->physical + offset) | 0x80000000u;
        __asm__ volatile("icbi @%0" ::"r"(address) : "memory");
    }
    __asm__ volatile("synco" ::: "memory");
#endif
    memory->loaded = 1;
    return 0;
}

int edl_memory_activate(struct edl_memory *memory)
{
    if (!owned(memory) || memory->mapped || !memory->loaded)
        return -1;
    for (unsigned i = 0; i < EDL_MEMORY_PAGES; i++)
    {
        uint32_t offset = i * EDL_MEMORY_PAGE;
        if (memory->ops.map(memory->ops.context, EDL_MEMORY_VBASE + offset,
                            memory->physical + offset, EDL_MEMORY_PAGE))
        {
            if (edl_memory_close(memory))
                return -2; /* Stale TLB: allocated frames remain pinned. */
            return -1;
        }
        memory->mapped++;
    }
    return 0;
}

int edl_memory_close(struct edl_memory *memory)
{
    if (!memory || !memory->allocation)
        return -1;
    while (memory->mapped)
    {
        uint32_t offset = (memory->mapped - 1u) * EDL_MEMORY_PAGE;
        if (memory->ops.unmap(memory->ops.context, EDL_MEMORY_VBASE + offset, EDL_MEMORY_PAGE))
        {
            memory->poisoned = 1;
            return -1;
        }
        memory->mapped--;
    }
    memory->ops.free(memory->ops.context, memory->allocation);
    memset(memory, 0, sizeof *memory);
    return 0;
}
