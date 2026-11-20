#include "elf.h"
#include <string.h>

static uint16_t u16(const unsigned char *p)
{ return ((uint16_t)p[0] << 8) | p[1]; }
static uint32_t u32(const unsigned char *p)
{ return ((uint32_t)u16(p) << 16) | u16(p + 2); }
static int within(uint32_t offset, uint32_t size, uint32_t limit)
{ return offset <= limit && size <= limit - offset; }

int edl_elf_inspect(edl_read_fn read, void *ctx, uint32_t size,
    uint32_t start, uint32_t end, struct edl_image *out)
{
    unsigned char h[52], p[32];
    struct edl_image image = {0};
    if(!read || !out || start >= end || size < sizeof h ||
        read(ctx, 0, h, sizeof h)) return -1;
    if(memcmp(h, "\177ELF\1\2\1", 7) || u16(h+16) != 2 ||
        u16(h+18) != 42 || u32(h+20) != 1 || u16(h+40) != 52 ||
        u16(h+42) != 32) return -1;
    uint32_t phoff = u32(h+28);
    unsigned phnum = u16(h+44);
    if(!phnum || phnum > 128 || !within(phoff, phnum * 32u, size)) return -1;
    image.entry = u32(h+24);
    int entry_ok = 0;
    for(unsigned i = 0; i < phnum; i++) {
        if(read(ctx, phoff + i * 32u, p, sizeof p)) return -1;
        uint32_t type = u32(p);
        /* Dynamic linking, interpreters and TLS need runtime support that
           this fixed-address loader does not provide. */
        if(type == 2 || type == 3 || type == 7) return -1;
        if(type != 1) continue;
        if(image.count == EDL_MAX_SEGMENTS) return -1;
        struct edl_segment s = {u32(p+4), u32(p+8), u32(p+16),
            u32(p+20), u32(p+24)};
        uint32_t align = u32(p+28);
        if(!s.memory_size && !s.file_size) continue;
        if(!s.memory_size || s.file_size > s.memory_size ||
            !within(s.offset, s.file_size, size) || s.address < start ||
            s.address >= end || s.memory_size > end - s.address ||
            (align > 1 && ((align & (align-1)) ||
                ((s.address - s.offset) & (align-1))))) return -1;
        for(unsigned j = 0; j < image.count; j++) {
            const struct edl_segment *q = &image.segments[j];
            if(s.address < q->address + q->memory_size &&
                q->address < s.address + s.memory_size) return -1;
        }
        if((s.flags & 1) && image.entry >= s.address &&
            image.entry - s.address < s.file_size && !(image.entry & 1))
            entry_ok = 1;
        image.segments[image.count++] = s;
    }
    if(!entry_ok) return -1;
    *out = image;
    return 0;
}

int edl_elf_metadata(edl_read_fn read, void *ctx, uint32_t size,
    void *out, size_t out_size)
{
    unsigned char h[52], s[40], names[40], name[10];
    if(!read || !out || size<sizeof h || read(ctx,0,h,sizeof h) ||
        memcmp(h,"\177ELF\1\2\1",7) || u16(h+18)!=42 ||
        u16(h+46)!=40) return -1;
    uint32_t base=u32(h+32);
    unsigned count=u16(h+48), strings=u16(h+50);
    if(!count || count>512 || strings>=count ||
        !within(base,count*40u,size) ||
        read(ctx,base+strings*40u,names,sizeof names)) return -1;
    uint32_t names_offset=u32(names+16), names_size=u32(names+20);
    if(u32(names+4)!=3 || !within(names_offset,names_size,size)) return -1;
    uint32_t found=0;
    for(unsigned i=0;i<count;i++) {
        if(read(ctx,base+i*40u,s,sizeof s)) return -1;
        uint32_t n=u32(s);
        if(!within(n,sizeof name,names_size)) continue;
        if(read(ctx,names_offset+n,name,sizeof name)) return -1;
        if(memcmp(name,".edl.meta",10)) continue;
        if(found || u32(s+4)!=1 || (u32(s+8)&2) ||
            u32(s+20)!=out_size || !within(u32(s+16),u32(s+20),size)) return -1;
        found=u32(s+16);
    }
    if(!found || read(ctx,found,out,out_size)) return -1;
    unsigned char *bytes=out;
    return out_size>=8 && u32(bytes)==0x45444c31 && u32(bytes+4)==1 ? 0 : -1;
}

int edl_elf_load(edl_read_fn read, void *rctx, uint32_t size,
    uint32_t start, uint32_t end, edl_write_fn write, void *wctx,
    struct edl_image *out)
{
    struct edl_image image;
    unsigned char chunk[512];
    if(!write || !out || edl_elf_inspect(read,rctx,size,start,end,&image)) return -1;
    for(unsigned i=0;i<image.count;i++) {
        const struct edl_segment *s=&image.segments[i];
        uint32_t at=0;
        while(at<s->file_size) {
            size_t n=s->file_size-at;
            if(n>sizeof chunk) n=sizeof chunk;
            if(read(rctx,s->offset+at,chunk,n) || write(wctx,s->address+at,chunk,n)) return -1;
            at+=(uint32_t)n;
        }
        memset(chunk,0,sizeof chunk);
        while(at<s->memory_size) {
            size_t n=s->memory_size-at;
            if(n>sizeof chunk) n=sizeof chunk;
            if(write(wctx,s->address+at,chunk,n)) return -1;
            at+=(uint32_t)n;
        }
    }
    *out=image;
    return 0;
}

