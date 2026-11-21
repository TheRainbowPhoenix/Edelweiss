#include "../exword/memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct mock
{
    uintptr_t answer;
    size_t requested;
    int allocations, releases, maps, unmaps;
    int map_fail_at, unmap_fail_at;
    uint32_t mapped_va[EDL_MEMORY_PAGES], mapped_pa[EDL_MEMORY_PAGES];
};
static void *allocate(void *context, size_t size)
{
    struct mock *m = context;
    m->requested = size;
    m->allocations++;
    return (void *)m->answer;
}
static void release(void *context, void *p)
{
    struct mock *m = context;
    assert((uintptr_t)p == m->answer);
    m->releases++;
}
static int map(void *context, uint32_t va, uint32_t pa, uint32_t bytes)
{
    struct mock *m = context;
    assert(bytes == EDL_MEMORY_PAGE);
    if (m->maps == m->map_fail_at)
        return -1;
    assert(m->maps < EDL_MEMORY_PAGES);
    m->mapped_va[m->maps] = va;
    m->mapped_pa[m->maps] = pa;
    m->maps++;
    return 0;
}
static int unmap(void *context, uint32_t va, uint32_t bytes)
{
    struct mock *m = context;
    assert(bytes == EDL_MEMORY_PAGE);
    if (m->unmaps == m->unmap_fail_at)
        return -1;
    assert(m->unmaps < m->maps);
    assert(va == m->mapped_va[m->maps - 1 - m->unmaps]);
    m->unmaps++;
    return 0;
}
static struct edl_memory_ops ops(struct mock *m)
{
    return (struct edl_memory_ops){
        .context = m, .alloc = allocate, .free = release, .map = map, .unmap = unmap};
}
int main(void)
{
    uint32_t physical = 0;
    assert(!edl_memory_physical((void *)(uintptr_t)0x8c011234u,
                                EDL_MEMORY_BYTES + EDL_MEMORY_PAGE - 1u, &physical));
    assert(physical == 0x0c020000u);
    assert(edl_memory_physical((void *)(uintptr_t)0x00300000u,
                               EDL_MEMORY_BYTES + EDL_MEMORY_PAGE - 1u, &physical) < 0);
    assert(edl_memory_physical((void *)(uintptr_t)0x8ffff000u,
                               EDL_MEMORY_BYTES + EDL_MEMORY_PAGE - 1u, &physical) < 0);
    assert(edl_memory_physical((void *)(uintptr_t)0x8c011234u, 0x80000u, &physical) < 0);

    struct mock m = {.answer = 0x8c011234u, .map_fail_at = -1, .unmap_fail_at = -1};
    struct edl_memory memory;
    struct edl_memory_ops callbacks = ops(&m);
    assert(!edl_memory_open(&memory, &callbacks));
    assert(m.requested == EDL_MEMORY_BYTES + EDL_MEMORY_PAGE - 1u);
    assert(!edl_memory_resolve(&memory, EDL_MEMORY_VBASE + 8, 12, &physical));
    assert(physical == 0x0c020008u);
    assert(edl_memory_resolve(&memory, EDL_MEMORY_VBASE - 4, 4, &physical) < 0);
    assert(edl_memory_resolve(&memory, EDL_MEMORY_STACK_END - 4, 8, &physical) < 0);
    assert(!edl_memory_activate(&memory));
    assert(m.maps == EDL_MEMORY_PAGES);
    for (unsigned i = 0; i < EDL_MEMORY_PAGES; i++)
    {
        assert(m.mapped_va[i] == EDL_MEMORY_VBASE + i * EDL_MEMORY_PAGE);
        assert(m.mapped_pa[i] == 0x0c020000u + i * EDL_MEMORY_PAGE);
    }
    assert(edl_memory_activate(&memory) < 0);
    assert(!edl_memory_close(&memory));
    assert(m.unmaps == EDL_MEMORY_PAGES && m.releases == 1);

    memset(&m, 0, sizeof m);
    m.answer = 0x8c011234u;
    m.map_fail_at = 3;
    m.unmap_fail_at = -1;
    callbacks = ops(&m);
    assert(!edl_memory_open(&memory, &callbacks));
    assert(edl_memory_activate(&memory) < 0);
    assert(m.maps == 3 && m.unmaps == 3 && m.releases == 1);

    memset(&m, 0, sizeof m);
    m.answer = 0x8c011234u;
    m.map_fail_at = -1;
    m.unmap_fail_at = 0;
    callbacks = ops(&m);
    assert(!edl_memory_open(&memory, &callbacks));
    assert(!edl_memory_activate(&memory));
    assert(edl_memory_close(&memory) < 0);
    assert(memory.poisoned && m.releases == 0);
    assert(edl_memory_activate(&memory) < 0);
    puts("Allocator-backed P0 mapping, rollback and fail-closed teardown passed");
    return 0;
}
