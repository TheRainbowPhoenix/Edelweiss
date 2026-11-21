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

/* All callbacks are supplied by the platform. alloc() must reserve RAM with
   a matching free(). If translate is NULL, alloc must return contiguous P1
   physical RAM; otherwise translate must resolve each 4 KiB virtual page in
   the allocator's original OS world, enabling non-adjacent 64 KiB extents.
   Each 64 KiB backing extent MUST be physically contiguous and aligned.
   map()/unmap() must own the entire P0 virtual range and preserve all OS
   and gint mappings; unmap success certifies UTLB AND ITLB removal.
   The small resident gint heap is NOT suitable for backing a 1 MiB image. */
struct edl_memory_ops
{
    void *context;
    void *(*alloc)(void *context, size_t size);
    void (*free)(void *context, void *allocation);
    /* Optional: translate 4 KiB-aligned OS VA to 4 KiB-aligned RAM PA.
       Called before leaving OS world; no TLB changes are permitted here. */
    int (*translate)(void *context, uint32_t va, uint32_t *physical);
    int (*map)(void *context, uint32_t va, uint32_t physical, uint32_t size);
    int (*unmap)(void *context, uint32_t va, uint32_t size);
};

struct edl_memory
{
    struct edl_memory_ops ops;
    void *allocation;
    uint32_t physical;
    uint32_t physical_pages[EDL_MEMORY_PAGES];
    unsigned mapped;
    int loaded;
    int poisoned;
};

/* Checks an allocation without dereferencing it; usable in host tests. */
int edl_memory_physical(void *allocation, size_t allocated_bytes, uint32_t *physical);
/* Reserve OS-owned RAM (not yet executable). The caller must keep it pinned. */
int edl_memory_open(struct edl_memory *memory, const struct edl_memory_ops *ops);
/* Resolve a destination within one 64 KiB backing extent. Cross-page
   accesses must be split by the caller (edl_memory_write does this). */
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
