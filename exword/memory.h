#ifndef EDL_MEMORY_H
#define EDL_MEMORY_H
#include <stddef.h>
#include <stdint.h>

/* A fixed 1 MiB P0 window: 960 KiB image, a 32 KiB guard gap and
   32 KiB stack. A SH7305 can map it with sixteen 64 KiB UTLB entries. */
#define EDL_MEMORY_VBASE 0x00300000u
#define EDL_MEMORY_PAGE 0x00010000u
#define EDL_MEMORY_BYTES 0x00100000u
#define EDL_MEMORY_IMAGE_END (EDL_MEMORY_VBASE + 0x000f0000u)
#define EDL_MEMORY_STACK_START (EDL_MEMORY_VBASE + 0x000f8000u)
#define EDL_MEMORY_STACK_END (EDL_MEMORY_VBASE + EDL_MEMORY_BYTES)
#define EDL_MEMORY_PAGES (EDL_MEMORY_BYTES / EDL_MEMORY_PAGE)

/* All callbacks are supplied by the platform. alloc() must return an
   owned, physically contiguous P1 RAM block. map()/unmap() must reserve and
   restore TLB ownership; neither function may invalidate OS-owned slots.
   Returning success from unmap() certifies that both UTLB and ITLB aliases
   have been removed. A platform must NOT use the tiny resident gint heap. */
struct edl_memory_ops
{
    void *context;
    void *(*alloc)(void *context, size_t size);
    void (*free)(void *context, void *allocation);
    int (*map)(void *context, uint32_t va, uint32_t physical, uint32_t size);
    int (*unmap)(void *context, uint32_t va, uint32_t size);
};

struct edl_memory
{
    struct edl_memory_ops ops;
    void *allocation;
    uint32_t physical;
    unsigned mapped;
    int loaded;
    int poisoned;
};

/* Checks an allocation without dereferencing it; usable in host tests. */
int edl_memory_physical(void *allocation, size_t allocated_bytes, uint32_t *physical);
/* Reserve OS-owned RAM (not yet executable). The caller must keep it pinned. */
int edl_memory_open(struct edl_memory *memory, const struct edl_memory_ops *ops);
/* Translate a virtual destination into the owned physical backing range. */
int edl_memory_resolve(const struct edl_memory *memory, uint32_t va, size_t size,
                       uint32_t *physical);
/* Finish code cache coherency and arm the image for mapping. */
int edl_memory_finish_load(struct edl_memory *memory);
/* ELF writer: copies to the uncached P2 alias, without touching the TLB. */
int edl_memory_write(void *memory, uint32_t va, const void *data, size_t size);
/* Bind all 64 KiB mappings only after ELF validation, copying and fixups. */
int edl_memory_activate(struct edl_memory *memory);
/* Unmap before freeing. On unmap failure retain the allocation (fail closed). */
int edl_memory_close(struct edl_memory *memory);
#endif
