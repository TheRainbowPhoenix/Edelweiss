#include "../exword/elf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define BASE 0x8c700000u
#define ACTUAL 0x8c800000u
static unsigned char file[2048], memory[4096];
static unsigned writes;
static void w16(unsigned at, unsigned v)
{
    file[at] = v >> 8;
    file[at + 1] = v;
}
static void w32(unsigned at, uint32_t v)
{
    w16(at, v >> 16);
    w16(at + 2, v);
}
static uint32_t value(unsigned at)
{
    return (uint32_t)memory[at] << 24 | (uint32_t)memory[at + 1] << 16 |
           (uint32_t)memory[at + 2] << 8 | memory[at + 3];
}
static int read_file(void *ctx, uint32_t at, void *out, size_t n)
{
    (void)ctx;
    if (at > sizeof file || n > sizeof file - at)
        return -1;
    memcpy(out, file + at, n);
    return 0;
}
#define P0_ACTUAL 0x00300000u
static int write_p0(void *ctx, uint32_t at, const void *data, size_t n)
{
    (void)ctx;
    assert(at >= P0_ACTUAL && at < P0_ACTUAL + sizeof memory);
    assert(n <= P0_ACTUAL + sizeof memory - at);
    memcpy(memory + at - P0_ACTUAL, data, n);
    writes++;
    return 0;
}
static int write_memory(void *ctx, uint32_t at, const void *data, size_t n)
{
    (void)ctx;
    assert(at >= ACTUAL && at < ACTUAL + sizeof memory && n <= ACTUAL + sizeof memory - at);
    memcpy(memory + at - ACTUAL, data, n);
    writes++;
    return 0;
}
static void fixture(void)
{
    memset(file, 0, sizeof file);
    memset(memory, 0xa5, sizeof memory);
    writes = 0;
    memcpy(file, "\177ELF\1\2\1", 7);
    w16(16, 2);
    w16(18, 42);
    w32(20, 1);
    w32(24, BASE);
    w32(28, 52);
    w32(32, 256);
    w16(40, 52);
    w16(42, 32);
    w16(44, 1);
    w16(46, 40);
    w16(48, 7);
    w16(50, 1);
    w32(52, 1);
    w32(56, 128);
    w32(60, BASE);
    w32(68, 16);
    w32(72, 32);
    w32(76, 7);
    w32(80, 4);
    /* [1] strings, [2] text, [3] symtab, [4] rela.text, [5] marker, [6] REL */
    w32(296 + 4, 3);
    w32(296 + 16, 600);
    w32(296 + 20, 32);
    memcpy(file + 600, "\0.edl.reloc\0", 12);
    w32(336 + 4, 1);
    w32(336 + 8, 6);
    w32(336 + 12, BASE);
    w32(336 + 16, 128);
    w32(336 + 20, 16);
    w32(376 + 4, 2);
    w32(376 + 16, 700);
    w32(376 + 20, 48);
    w32(376 + 36, 16);
    w32(416 + 4, 4);
    w32(416 + 16, 800);
    w32(416 + 20, 36);
    w32(416 + 24, 3);
    w32(416 + 28, 2);
    w32(416 + 36, 12);
    w32(456, 1);
    w32(456 + 4, 1);
    w32(456 + 16, 640);
    w32(456 + 20, 12);
    w32(640, 0x45445231);
    w32(644, BASE);
    w32(648, 4096);
    /* One internal symbol, one absolute MMIO symbol. */
    w32(716 + 4, BASE);
    w16(716 + 14, 2);
    w32(732 + 4, 0xa4050000);
    w16(732 + 14, 0xfff1);
    w32(128, BASE + 20);
    w32(132, 0xa4050000);
    w32(136, 12);
    w32(800, BASE);
    w32(804, 0x101);
    w32(808, 20); /* internal pointer */
    w32(812, BASE + 4);
    w32(816, 0x201);
    w32(820, 0); /* MMIO */
    w32(824, BASE + 8);
    w32(828, 0x102);
    w32(832, 20); /* internal PC-relative */
}
/* Re-link the synthetic ET_EXEC image; relocations stay in place. */
static void fixture_at(uint32_t base)
{
    fixture();
    w32(24, base);
    w32(60, base);
    w32(336 + 12, base);
    w32(644, base);
    w32(716 + 4, base);
    w32(128, base + 20);
    w32(800, base);
    w32(812, base + 4);
    w32(824, base + 8);
}
static int load_auto(void)
{
    struct edl_image image;
    return edl_elf_load_auto(read_file, NULL, sizeof file, ACTUAL, ACTUAL + 4096, write_memory,
                             NULL, NULL, NULL, &image);
}
static int load(void)
{
    struct edl_image image;
    int rc = edl_elf_load_at(read_file, NULL, sizeof file, BASE, BASE + 4096, ACTUAL, write_memory,
                             NULL, &image);
    if (!rc)
    {
        assert(image.entry == ACTUAL && image.segments[0].address == ACTUAL);
        assert(value(0) == ACTUAL + 20 && value(4) == 0xa4050000 && value(8) == 12);
        assert(value(16) == 0 && memory[32] == 0xa5);
    }
    return rc;
}
static int resolve_service(void *ctx, const char *name, uint32_t *address)
{
    (void)ctx;
    if (strcmp(name, "_getkey"))
        return -1;
    *address = 0x70001000;
    return 0;
}
static void import_fixture(void)
{
    fixture();
    memcpy(file + 612, ".edl.gint", 10);
    memcpy(file + 623, "_getkey", 8);
    w32(496, 12);
    w32(500, 1);
    w32(512, 660);
    w32(516, 8);
    w32(660, 0x47494e54);
    w32(664, 1);
    w32(376 + 24, 1);
    w32(732, 23);
    w32(732 + 4, 0);
    w32(132, 8);
}
int main(void)
{
    uint32_t link_start, link_end;
    fixture();
    assert(!edl_elf_link_range(read_file, NULL, sizeof file, &link_start, &link_end));
    assert(link_start == BASE && link_end == BASE + 4096);
    assert(!load_auto());
    assert(value(0) == ACTUAL + 20 && value(4) == 0xa4050000);
    fixture();
    struct edl_image mapped_image;
    assert(!edl_elf_load_to(read_file, NULL, sizeof file, P0_ACTUAL, P0_ACTUAL + 4096, write_p0,
                            NULL, NULL, NULL, &mapped_image));
    assert(mapped_image.entry == P0_ACTUAL);
    assert(value(0) == P0_ACTUAL + 20 && value(4) == 0xa4050000);
    fixture();
    writes = 0;
    assert(edl_elf_load_to(read_file, NULL, sizeof file, P0_ACTUAL, P0_ACTUAL + 1024, write_p0,
                           NULL, NULL, NULL, &mapped_image) < 0 &&
           writes == 0);
    fixture_at(0x8c900000u);
    assert(!edl_elf_link_range(read_file, NULL, sizeof file, &link_start, &link_end));
    assert(link_start == 0x8c900000u && link_end == link_start + 4096);
    assert(!load_auto());
    assert(value(0) == ACTUAL + 20 && value(4) == 0xa4050000);
    fixture();
    w32(648, 8192);
    assert(load_auto() < 0 && writes == 0); /* Not enough device arena. */
    fixture();
    w32(648, 0);
    assert(edl_elf_link_range(read_file, NULL, sizeof file, &link_start, &link_end) < 0);
    assert(load_auto() < 0 && writes == 0);
    fixture();
    w32(644, 0xfffff000u);
    assert(edl_elf_link_range(read_file, NULL, sizeof file, &link_start, &link_end) < 0);
    assert(load_auto() < 0 && writes == 0);
    fixture();
    w32(640, 0);
    assert(load_auto() < 0 && writes == 0);
    fixture();
    assert(!load());
    fixture();
    w32(808, 0);
    w32(832, 0);
    assert(!load()); /* GNU SH in-place addends. */
    fixture();
    w32(828, 0x202);
    w32(832, 0);
    w32(136, 0xa4050000 - (BASE + 8));
    struct edl_image image;
    assert(!edl_elf_load_at(read_file, NULL, sizeof file, BASE, BASE + 4096, ACTUAL, write_memory,
                            NULL, &image));
    assert(value(8) == 0xa4050000 - (ACTUAL + 8));
    fixture();
    w32(416 + 4, 9);
    w32(416 + 20, 8);
    w32(416 + 36, 8);
    assert(!load());
    /* Reject all malformed relocation data before the first RAM write. */
    const unsigned offsets[] = {804,      800, 716 + 14, 416 + 24, 416 + 28, 416 + 36,
                                456 + 20, 640, 644,      648,      128};
    const uint32_t invalid[] = {0x103, BASE + 16, 0, 99, 99, 8, 13, 0, BASE + 4, 8192, 0};
    for (unsigned i = 0; i < sizeof offsets / sizeof *offsets; i++)
    {
        fixture();
        w32(offsets[i], invalid[i]);
        assert(load() < 0 && writes == 0);
    }
    fixture();
    w32(80, 0x200000);
    assert(load() < 0 && writes == 0);
    fixture();
    assert(edl_elf_load_at(read_file, NULL, sizeof file, BASE, BASE + 4096, ACTUAL + 2,
                           write_memory, NULL, &image) < 0 &&
           writes == 0);
    fixture();
    w16(48, 0);
    assert(load() < 0 && writes == 0);
    fixture();
    w32(416 + 20, 0);
    assert(load() < 0 && writes == 0);
    fixture();
    assert(edl_elf_load_at(read_file, NULL, sizeof file, BASE, BASE + 4096, 0xfffff800,
                           write_memory, NULL, &image) < 0 &&
           writes == 0);
    import_fixture();
    struct edl_image auto_image;
    assert(!edl_elf_load_auto(read_file, NULL, sizeof file, ACTUAL, ACTUAL + 4096, write_memory,
                              NULL, resolve_service, NULL, &auto_image));
    assert(value(4) == 0x70001008 && auto_image.entry == ACTUAL);
    import_fixture();
    assert(!edl_elf_load_linked(read_file, NULL, sizeof file, BASE, BASE + 4096, ACTUAL,
                                write_memory, NULL, resolve_service, NULL, &image));
    assert(value(4) == 0x70001008 && value(0) == ACTUAL + 20);
    import_fixture();
    assert(edl_elf_load_at(read_file, NULL, sizeof file, BASE, BASE + 4096, ACTUAL, write_memory,
                           NULL, &image) < 0 &&
           writes == 0);
    import_fixture();
    file[624] = 'X';
    assert(edl_elf_load_linked(read_file, NULL, sizeof file, BASE, BASE + 4096, ACTUAL,
                               write_memory, NULL, resolve_service, NULL, &image) < 0 &&
           writes == 0);
    puts("ELF relocation, fixed MMIO, REL32, BSS and malformed input checks passed");
}
