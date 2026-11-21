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

/* The host allocator can own a virtual block with non-adjacent 64 KiB frames.
   Validate *every* 4 KiB subpage before trusting each 64 KiB translation. */
static int backing_pages(const struct edl_memory_ops *ops, void *p, size_t bytes,
                         uint32_t pages[EDL_MEMORY_PAGES])
{
    uintptr_t raw = (uintptr_t)p;
    uint32_t first = 0;
    if (!ops->translate)
    {
        if (edl_memory_physical(p, bytes, &first))
            return -1;
        for (unsigned i = 0; i < EDL_MEMORY_PAGES; i++)
            pages[i] = first + i * EDL_MEMORY_PAGE;
        return 0;
    }
    if (raw < 0x00010000u || raw >= 0xa0000000u || bytes > UINT32_MAX ||
        raw > UINT32_MAX - bytes)
        return -1;
    uint32_t aligned = ((uint32_t)raw + EDL_MEMORY_PAGE - 1u) &
                       ~(EDL_MEMORY_PAGE - 1u);
    if (aligned < raw || aligned - raw > bytes ||
        EDL_MEMORY_BYTES > bytes - (aligned - raw))
        return -1;
    for (unsigned i = 0; i < EDL_MEMORY_PAGES; i++)
    {
        uint32_t base = 0;
        for (unsigned j = 0; j < EDL_MEMORY_PAGE / 4096u; j++)
        {
            uint32_t address = aligned + i * EDL_MEMORY_PAGE + j * 4096u;
            uint32_t physical = 0;
            if (ops->translate(ops->context, address, &physical) ||
                (physical & 4095u) || physical < 0x0c000000u ||
                physical > 0x10000000u - 4096u)
                return -1;
            if (!j)
            {
                if (physical & (EDL_MEMORY_PAGE - 1u))
                    return -1;
                base = physical;
            }
            else if (physical != base + j * 4096u)
                return -1;
        }
        pages[i] = base;
    }
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
    if (backing_pages(ops, p, bytes, memory->physical_pages))
    {
        ops->free(ops->context, p);
        return -1;
    }
    memory->allocation = p;
    memory->physical = memory->physical_pages[0];
#if defined(__sh__)
    /* Evict any dirty cached aliases before the ELF writer uses P2. */
    for (unsigned i = 0; i < EDL_MEMORY_PAGES; i++)
        for (uint32_t offset = 0; offset < EDL_MEMORY_PAGE; offset += 32)
        {
            uint32_t address = (memory->physical_pages[i] + offset) | 0x80000000u;
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
    uint32_t page_offset = offset % EDL_MEMORY_PAGE;
    if (size > EDL_MEMORY_PAGE - page_offset)
        return -1;
    *physical = memory->physical_pages[offset / EDL_MEMORY_PAGE] + page_offset;
    return 0;
}

int edl_memory_write(void *memory, uint32_t va, const void *data, size_t size)
{
    struct edl_memory *owned_memory = memory;
    uint32_t physical;
    if (!owned(owned_memory) || owned_memory->mapped || owned_memory->loaded || !data ||
        va < EDL_MEMORY_VBASE || va >= EDL_MEMORY_STACK_END ||
        size > EDL_MEMORY_STACK_END - va)
        return -1;
#if defined(__sh__)
    /* Copy per extent because host-owned backing can be physically scattered.
       It remains pinned until ALL mapped TLB aliases are removed. */
    const unsigned char *src = data;
    while (size)
    {
        uint32_t page_offset = (va - EDL_MEMORY_VBASE) % EDL_MEMORY_PAGE;
        size_t piece = EDL_MEMORY_PAGE - page_offset;
        if (piece > size)
            piece = size;
        if (edl_memory_resolve(memory, va, piece, &physical))
            return -1;
        memcpy((void *)(uintptr_t)(physical | 0xa0000000u), src, piece);
        src += piece;
        va += piece;
        size -= piece;
    }
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
    for (unsigned i = 0; i < EDL_MEMORY_PAGES; i++)
        for (uint32_t offset = 0; offset < EDL_MEMORY_PAGE; offset += 32)
        {
            uint32_t address = (memory->physical_pages[i] + offset) | 0x80000000u;
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
                            memory->physical_pages[i], EDL_MEMORY_PAGE))
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
