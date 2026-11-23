#include "../exword/read_cache.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned char source[16387];
static unsigned reads;
static int fail;
static int upstream(void *ctx, uint32_t offset, void *dest, size_t length)
{
    (void)ctx;
    reads++;
    if (fail || offset > sizeof source || length > sizeof source - offset)
        return -1;
    memcpy(dest, source + offset, length);
    return 0;
}

int main(void)
{
    for (unsigned i = 0; i < sizeof source; i++)
        source[i] = (unsigned char)(i * 17u + i / 23u);
    static struct edl_read_cache cache;
    edl_read_cache_init(&cache, upstream, NULL, sizeof source);
    uint8_t buffer[4097];
    /* Thousands of small relocation reads within one 2 KiB block must
       be served by one native read, not thousands of OS seeks. */
    for (unsigned i = 0; i < 2000; i++)
    {
        unsigned at = 400 + i % 1000;
        assert(!edl_read_cache_read(&cache, at, buffer, 9));
        assert(!memcmp(buffer, source + at, 9));
    }
    assert(reads == 1);
    assert(!edl_read_cache_read(&cache, 2035, buffer, sizeof buffer));
    assert(!memcmp(buffer, source + 2035, sizeof buffer));
    assert(reads <= 4);
    for (unsigned i = 0; i < sizeof source; i += 37)
    {
        unsigned n = sizeof source - i < sizeof buffer ? sizeof source - i : sizeof buffer;
        assert(!edl_read_cache_read(&cache, i, buffer, n));
        assert(!memcmp(buffer, source + i, n));
    }
    /* A relocation pass reads more than four hot 2 KiB regions:
       .rela.text, .symtab, .text and .shstrtab. Eight slots must avoid
       repeated OS reads when six regions are revisited. */
    edl_read_cache_init(&cache, upstream, NULL, sizeof source);
    reads = 0;
    for (unsigned loop = 0; loop < 30; loop++)
        for (unsigned i = 0; i < 6; i++)
        {
            unsigned at = i * EDL_READ_CACHE_BLOCK + 25;
            assert(!edl_read_cache_read(&cache, at, buffer, 8));
            assert(!memcmp(buffer, source + at, 8));
        }
    assert(reads == 6); /* Four cache slots would require 180 reads. */
    assert(edl_read_cache_read(&cache, sizeof source - 2, buffer, 3) < 0);
    assert(edl_read_cache_read(&cache, UINT32_MAX, buffer, 2) < 0);
    assert(edl_read_cache_read(&cache, 0, NULL, 1) < 0);
    assert(!edl_read_cache_read(&cache, sizeof source, NULL, 0));
    edl_read_cache_init(&cache, upstream, NULL, sizeof source);
    fail = 1;
    assert(edl_read_cache_read(&cache, 2000, buffer, 4) < 0);
    fail = 0;
    assert(!edl_read_cache_read(&cache, 2000, buffer, 4));
    assert(!memcmp(buffer, source + 2000, 4));
    edl_read_cache_init(&cache, upstream, NULL, 8);
    assert(edl_read_cache_read(&cache, 7, buffer, 2) < 0);
    puts("ELF read cache: bounded reads, eviction, errors and metadata passed");
    return 0;
}
