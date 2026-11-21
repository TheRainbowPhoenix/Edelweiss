#include "read_cache.h"
#include <string.h>

void edl_read_cache_init(struct edl_read_cache *cache, edl_read_fn upstream, void *context,
                         uint32_t length)
{
    if (!cache)
        return;
    memset(cache, 0, sizeof *cache);
    cache->upstream = upstream;
    cache->context = context;
    cache->length = length;
}

int edl_read_cache_read(void *ctx, uint32_t offset, void *destination, size_t size)
{
    struct edl_read_cache *cache = ctx;
    if (!cache || !cache->upstream || (size && !destination) || offset > cache->length ||
        size > cache->length - offset)
        return -1;
    unsigned char *dst = destination;
    while (size)
    {
        uint32_t base = offset & ~(EDL_READ_CACHE_BLOCK - 1u);
        unsigned index = EDL_READ_CACHE_SLOTS;
        for (unsigned i = 0; i < EDL_READ_CACHE_SLOTS; i++)
        {
            if (cache->slots[i].length && cache->slots[i].offset == base)
            {
                index = i;
                break;
            }
        }
        if (index == EDL_READ_CACHE_SLOTS)
        {
            /* Choose an unused slot, then the least recently used slot. */
            index = 0;
            for (unsigned i = 0; i < EDL_READ_CACHE_SLOTS; i++)
                if (!cache->slots[i].length ||
                    cache->slots[i].age < cache->slots[index].age)
                {
                    index = i;
                    if (!cache->slots[i].length)
                        break;
                }
            struct edl_read_cache_slot *slot = &cache->slots[index];
            slot->length = 0;
            uint32_t count = cache->length - base;
            if (count > EDL_READ_CACHE_BLOCK)
                count = EDL_READ_CACHE_BLOCK;
            if (cache->upstream(cache->context, base, slot->data, count))
                return -1;
            slot->offset = base;
            slot->length = count;
        }
        struct edl_read_cache_slot *slot = &cache->slots[index];
        slot->age = ++cache->age;
        uint32_t at = offset - base;
        size_t take = slot->length - at;
        if (take > size)
            take = size;
        memcpy(dst, slot->data + at, take);
        dst += take;
        offset += (uint32_t)take;
        size -= take;
    }
    return 0;
}
