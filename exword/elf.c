#include "elf.h"
#include <string.h>

static uint16_t u16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}
static uint32_t u32(const unsigned char *p)
{
    return ((uint32_t)u16(p) << 16) | u16(p + 2);
}
static int within(uint32_t offset, uint32_t size, uint32_t limit)
{
    return offset <= limit && size <= limit - offset;
}

int edl_elf_inspect(edl_read_fn read, void *ctx, uint32_t size, uint32_t start, uint32_t end,
                    struct edl_image *out)
{
    unsigned char h[52], p[32];
    struct edl_image image = {0};
    if (!read || !out || start >= end || size < sizeof h || read(ctx, 0, h, sizeof h))
        return -1;
    if (memcmp(h, "\177ELF\1\2\1", 7) || u16(h + 16) != 2 || u16(h + 18) != 42 ||
        u32(h + 20) != 1 || u16(h + 40) != 52 || u16(h + 42) != 32)
        return -1;
    uint32_t phoff = u32(h + 28);
    unsigned phnum = u16(h + 44);
    if (!phnum || phnum > 128 || !within(phoff, phnum * 32u, size))
        return -1;
    image.entry = u32(h + 24);
    int entry_ok = 0;
    for (unsigned i = 0; i < phnum; i++)
    {
        if (read(ctx, phoff + i * 32u, p, sizeof p))
            return -1;
        uint32_t type = u32(p);
        /* Dynamic linking, interpreters and TLS need runtime support that
           this fixed-address loader does not provide. */
        if (type == 2 || type == 3 || type == 7)
            return -1;
        if (type != 1)
            continue;
        if (image.count == EDL_MAX_SEGMENTS)
            return -1;
        struct edl_segment s = {u32(p + 4), u32(p + 8), u32(p + 16), u32(p + 20), u32(p + 24)};
        uint32_t align = u32(p + 28);
        if (!s.memory_size && !s.file_size)
            continue;
        if (!s.memory_size || s.file_size > s.memory_size || !within(s.offset, s.file_size, size) ||
            s.address < start || s.address >= end || s.memory_size > end - s.address ||
            (align > 1 && ((align & (align - 1)) || ((s.address - s.offset) & (align - 1)))))
            return -1;
        for (unsigned j = 0; j < image.count; j++)
        {
            const struct edl_segment *q = &image.segments[j];
            if (s.address < q->address + q->memory_size && q->address < s.address + s.memory_size)
                return -1;
        }
        if ((s.flags & 1) && image.entry >= s.address && image.entry - s.address < s.file_size &&
            !(image.entry & 1))
            entry_ok = 1;
        image.segments[image.count++] = s;
    }
    if (!entry_ok)
        return -1;
    *out = image;
    return 0;
}

int edl_elf_metadata(edl_read_fn read, void *ctx, uint32_t size, void *out, size_t out_size)
{
    unsigned char h[52], s[40], names[40], name[10];
    if (!read || !out || size < sizeof h || read(ctx, 0, h, sizeof h) ||
        memcmp(h, "\177ELF\1\2\1", 7) || u16(h + 18) != 42 || u16(h + 46) != 40)
        return -1;
    uint32_t base = u32(h + 32);
    unsigned count = u16(h + 48), strings = u16(h + 50);
    if (!count || count > 512 || strings >= count || !within(base, count * 40u, size) ||
        read(ctx, base + strings * 40u, names, sizeof names))
        return -1;
    uint32_t names_offset = u32(names + 16), names_size = u32(names + 20);
    if (u32(names + 4) != 3 || !within(names_offset, names_size, size))
        return -1;
    uint32_t found = 0;
    for (unsigned i = 0; i < count; i++)
    {
        if (read(ctx, base + i * 40u, s, sizeof s))
            return -1;
        uint32_t n = u32(s);
        if (!within(n, sizeof name, names_size))
            continue;
        if (read(ctx, names_offset + n, name, sizeof name))
            return -1;
        if (memcmp(name, ".edl.meta", 10))
            continue;
        if (found || u32(s + 4) != 1 || (u32(s + 8) & 2) || u32(s + 20) != out_size ||
            !within(u32(s + 16), u32(s + 20), size))
            return -1;
        found = u32(s + 16);
    }
    if (!found || read(ctx, found, out, out_size))
        return -1;
    unsigned char *bytes = out;
    return out_size >= 8 && u32(bytes) == 0x45444c31 && u32(bytes + 4) == 1 ? 0 : -1;
}

int edl_elf_load(edl_read_fn read, void *rctx, uint32_t size, uint32_t start, uint32_t end,
                 edl_write_fn write, void *wctx, struct edl_image *out)
{
    struct edl_image image;
    unsigned char chunk[512];
    if (!write || !out || edl_elf_inspect(read, rctx, size, start, end, &image))
        return -1;
    for (unsigned i = 0; i < image.count; i++)
    {
        const struct edl_segment *s = &image.segments[i];
        uint32_t at = 0;
        while (at < s->file_size)
        {
            size_t n = s->file_size - at;
            if (n > sizeof chunk)
                n = sizeof chunk;
            if (read(rctx, s->offset + at, chunk, n) || write(wctx, s->address + at, chunk, n))
                return -1;
            at += (uint32_t)n;
        }
        memset(chunk, 0, sizeof chunk);
        while (at < s->memory_size)
        {
            size_t n = s->memory_size - at;
            if (n > sizeof chunk)
                n = sizeof chunk;
            if (write(wctx, s->address + at, chunk, n))
                return -1;
            at += (uint32_t)n;
        }
    }
    *out = image;
    return 0;
}

struct relocation
{
    edl_resolve_fn resolve;
    void *resolve_context;
    int imports;
    edl_read_fn read;
    void *rctx;
    uint32_t size, base, end, delta, shoff;
    unsigned shnum;
    const struct edl_image *image;
    edl_write_fn write;
    void *wctx;
};
static int section(struct relocation *r, unsigned index, unsigned char s[40])
{
    return index >= r->shnum ? -1 : r->read(r->rctx, r->shoff + index * 40u, s, 40);
}
static int file_word(struct relocation *r, uint32_t address, uint32_t *offset)
{
    for (unsigned i = 0; i < r->image->count; i++)
    {
        const struct edl_segment *s = &r->image->segments[i];
        if (address >= s->address && s->file_size >= 4 && address - s->address <= s->file_size - 4)
        {
            *offset = s->offset + address - s->address;
            return 0;
        }
    }
    return -1;
}
static int relocation_marker(struct relocation *r, const unsigned char h[52], uint32_t *link_start,
                             uint32_t *link_end)
{
    unsigned char strings[40], s[40], name[11], marker[12];
    if (u16(h + 46) != 40 || !r->shnum || r->shnum > 512 ||
        !within(r->shoff, r->shnum * 40u, r->size) || section(r, u16(h + 50), strings) ||
        u32(strings + 4) != 3 || !within(u32(strings + 16), u32(strings + 20), r->size))
        return -1;
    unsigned found = 0;
    for (unsigned i = 0; i < r->shnum; i++)
    {
        if (section(r, i, s))
            return -1;
        if (u32(s) >= u32(strings + 20))
            continue;
        uint32_t name_size = u32(strings + 20) - u32(s);
        if (name_size > sizeof name)
            name_size = sizeof name;
        memset(name, 0, sizeof name);
        if (r->read(r->rctx, u32(strings + 16) + u32(s), name, name_size) ||
            !memchr(name, 0, name_size))
            continue;
        if (!memcmp(name, ".edl.gint", 10) && u32(s + 20) == 8)
        {
            unsigned char tag[8];
            if (r->imports || u32(s + 4) != 1 || (u32(s + 8) & 2) ||
                !within(u32(s + 16), 8, r->size) || r->read(r->rctx, u32(s + 16), tag, 8) ||
                u32(tag) != 0x47494e54 || u32(tag + 4) != 1)
                return -1;
            r->imports = 1;
        }
        if (memcmp(name, ".edl.reloc", 11))
            continue;
        if (found++ || u32(s + 4) != 1 || (u32(s + 8) & 2) || u32(s + 20) != 12 ||
            !within(u32(s + 16), 12, r->size) ||
            r->read(r->rctx, u32(s + 16), marker, sizeof marker) || u32(marker) != 0x45445231 ||
            !u32(marker + 8) || u32(marker + 4) > UINT32_MAX - u32(marker + 8))
            return -1;
        *link_start = u32(marker + 4);
        *link_end = *link_start + u32(marker + 8);
    }
    return found == 1 ? 0 : -1;
}
/* First pass validates every relocation without writing. GNU SH uses
   partial-in-place addends even in SHT_RELA. This is ET_EXEC, so the stored
   word is already fully linked: adjust it by the movement of S and P, never
   recompute S+A or guess pointers by scanning memory. See binutils bfd/elf32-sh.c
   get_howto_table(...)[R_SH_DIR32].partial_inplace. Metadata/debug relocations
   target non-allocated sections and are deliberately ignored. */
static int relocate(struct relocation *r, int apply)
{
    unsigned char s[40], target[40], symbols[40], sym[16], def[40], rel[12], word[4];
    unsigned total = 0, found = 0;
    for (unsigned i = 0; i < r->shnum; i++)
    {
        if (section(r, i, s))
            return -1;
        unsigned type = u32(s + 4), entsize = type == 4 ? 12 : 8;
        if (type != 4 && type != 9)
            continue;
        if (section(r, u32(s + 28), target))
            return -1;
        if (!(u32(target + 8) & 2))
            continue;
        if (u32(s + 36) != entsize || u32(s + 20) % entsize ||
            !within(u32(s + 16), u32(s + 20), r->size) || section(r, u32(s + 24), symbols) ||
            u32(symbols + 4) != 2 || u32(symbols + 36) != 16 || u32(symbols + 20) % 16 ||
            !within(u32(symbols + 16), u32(symbols + 20), r->size))
            return -1;
        for (uint32_t j = 0; j < u32(s + 20); j += entsize)
        {
            if (++total > 65536 || r->read(r->rctx, u32(s + 16) + j, rel, entsize))
                return -1;
            uint32_t address = u32(rel), info = u32(rel + 4), kind = info & 255;
            if (!kind)
                continue; /* R_SH_NONE */
            found++;
            if (kind != 1 && kind != 2)
                return -1; /* DIR32, REL32 only */
            uint32_t index = info >> 8, offset;
            if (index >= u32(symbols + 20) / 16 ||
                r->read(r->rctx, u32(symbols + 16) + index * 16, sym, sizeof sym) || address & 3 ||
                address < u32(target + 12) ||
                !within(address - u32(target + 12), 4, u32(target + 20)) ||
                file_word(r, address, &offset) || r->read(r->rctx, offset, word, 4))
                return -1;
            unsigned shndx = u16(sym + 14);
            int moves = 0;
            uint32_t binding = 0;
            if (shndx == 0xfff1)
            { /* SHN_ABS: imported services have a zero link-time value. */
                if (r->imports && !u32(sym + 4))
                {
                    unsigned char strings[40];
                    char name[96];
                    if (!r->resolve || section(r, u32(symbols + 24), strings) ||
                        u32(strings + 4) != 3 ||
                        !within(u32(strings + 16), u32(strings + 20), r->size) ||
                        u32(sym) >= u32(strings + 20))
                        return -1;
                    uint32_t n = u32(strings + 20) - u32(sym);
                    if (n > sizeof name)
                        n = sizeof name;
                    if (r->read(r->rctx, u32(strings + 16) + u32(sym), name, n) ||
                        !memchr(name, 0, n) || r->resolve(r->resolve_context, name, &binding) ||
                        !binding)
                        return -1;
                }
            }
            else
            {
                if (!shndx || section(r, shndx, def) || !(u32(def + 8) & 2) ||
                    u32(sym + 4) < r->base || u32(sym + 4) > r->end)
                    return -1;
                moves = 1;
            }
            uint32_t value = u32(word);
            if (moves)
            {
                uint32_t destination = kind == 1 ? value : value + address;
                if (destination < r->base || destination > r->end)
                    return -1;
            }
            /* REL32's place moves too; internal PC-relative references do
               not change, references to fixed absolute symbols do. */
            value += binding + (kind == 1 ? (moves ? r->delta : 0) : (moves ? 0 : -r->delta));
            if (apply)
            {
                word[0] = value >> 24;
                word[1] = value >> 16;
                word[2] = value >> 8;
                word[3] = value;
                if (r->write(r->wctx, address + r->delta, word, 4))
                    return -1;
            }
        }
    }
    return found ? 0 : -1;
}
static int moved_write(void *ctx, uint32_t address, const void *data, size_t size)
{
    struct relocation *r = ctx;
    return r->write(r->wctx, address + r->delta, data, size);
}
int edl_elf_link_range(edl_read_fn read, void *rctx, uint32_t size, uint32_t *start, uint32_t *end)
{
    unsigned char h[52];
    if (!read || !start || !end || size < sizeof h || read(rctx, 0, h, sizeof h) ||
        memcmp(h, "\177ELF\1\2\1", 7) || u16(h + 16) != 2 || u16(h + 18) != 42 ||
        u32(h + 20) != 1 || u16(h + 40) != 52 || u16(h + 42) != 32)
        return -1;
    struct relocation r = {
        .read = read, .rctx = rctx, .size = size, .shoff = u32(h + 32), .shnum = u16(h + 48)};
    return relocation_marker(&r, h, start, end);
}

int edl_elf_load_auto(edl_read_fn read, void *rctx, uint32_t size, uint32_t arena_start,
                      uint32_t arena_end, edl_write_fn write, void *wctx, edl_resolve_fn resolve,
                      void *resolve_context, struct edl_image *out)
{
    uint32_t link_start, link_end;
    /* P1 is directly mapped; do not steal OS UTLB entries or guess RAM. */
    if (arena_start < 0x80000000u || arena_end > 0xa0000000u || arena_start >= arena_end ||
        edl_elf_link_range(read, rctx, size, &link_start, &link_end) || link_start < 0x80000000u ||
        link_end > 0xa0000000u || link_end - link_start > arena_end - arena_start)
        return -1;
    return edl_elf_load_linked(read, rctx, size, link_start, link_end, arena_start, write, wctx,
                               resolve, resolve_context, out);
}

int edl_elf_load_linked(edl_read_fn read, void *rctx, uint32_t size, uint32_t start, uint32_t end,
                        uint32_t actual, edl_write_fn write, void *wctx, edl_resolve_fn resolve,
                        void *resolve_context, struct edl_image *out)
{
    if (!out || !write || start >= end || actual > UINT32_MAX - (end - start))
        return -1;
    if (actual == start && !resolve)
        return edl_elf_load(read, rctx, size, start, end, write, wctx, out);
    struct edl_image image;
    unsigned char h[52];
    if (edl_elf_inspect(read, rctx, size, start, end, &image) || read(rctx, 0, h, sizeof h))
        return -1;
    for (unsigned i = 0; i < u16(h + 44); i++)
    {
        unsigned char ph[32];
        if (read(rctx, u32(h + 28) + i * 32u, ph, sizeof ph))
            return -1;
        uint32_t align = u32(ph + 28);
        if (u32(ph) == 1 && align > 1 && ((actual - start) & (align - 1)))
            return -1;
    }
    struct relocation r = {.resolve = resolve,
                           .resolve_context = resolve_context,
                           .read = read,
                           .rctx = rctx,
                           .size = size,
                           .base = start,
                           .end = end,
                           .delta = actual - start,
                           .shoff = u32(h + 32),
                           .shnum = u16(h + 48),
                           .image = &image,
                           .write = write,
                           .wctx = wctx};
    uint32_t marker_start, marker_end;
    if (relocation_marker(&r, h, &marker_start, &marker_end) || marker_start != start ||
        marker_end != end || relocate(&r, 0))
        return -1;
    if (edl_elf_load(read, rctx, size, start, end, moved_write, &r, &image) || relocate(&r, 1))
        return -1;
    image.entry += r.delta;
    for (unsigned i = 0; i < image.count; i++)
        image.segments[i].address += r.delta;
    *out = image;
    return 0;
}

int edl_elf_load_at(edl_read_fn read, void *rctx, uint32_t size, uint32_t start, uint32_t end,
                    uint32_t actual, edl_write_fn write, void *wctx, struct edl_image *out)
{
    return edl_elf_load_linked(read, rctx, size, start, end, actual, write, wctx, NULL, NULL, out);
}
