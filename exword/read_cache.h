#ifndef EDL_READ_CACHE_H
#define EDL_READ_CACHE_H

#include "elf.h"
#include <stdint.h>

/* All file I/O remains in the native OS world; this cache only reduces
   repeated small random reads during the ELF relocation passes. */
#define EDL_READ_CACHE_BLOCK 1024u
/* Keep the SAME total 8 KiB resident BSS budget as the original 2 KiB x 4 cache.
   Eight 1 KiB slots better retain separate ELF sections during relocation. */
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
