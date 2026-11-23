#ifndef EDL_READ_CACHE_H
#define EDL_READ_CACHE_H

#include "elf.h"
#include <stdint.h>

/* All file I/O remains in the native OS world; this cache only reduces
   repeated small random reads during the ELF relocation passes. */
#define EDL_READ_CACHE_BLOCK 2048u
/* Keep metadata, relocations, symbols and text hot across both ELF passes.
   The old four slots thrashed on the 3,160-relocation MicroQuickJS ELF. */
#define EDL_READ_CACHE_SLOTS 8u

struct edl_read_cache_slot
{
    uint32_t offset, length, age;
    unsigned char data[EDL_READ_CACHE_BLOCK];
};

struct edl_read_cache
{
    edl_read_fn upstream;
    void *context;
    uint32_t length, age;
    struct edl_read_cache_slot slots[EDL_READ_CACHE_SLOTS];
};

void edl_read_cache_init(struct edl_read_cache *cache, edl_read_fn upstream, void *context,
                         uint32_t length);
int edl_read_cache_read(void *cache, uint32_t offset, void *destination, size_t size);

#endif
