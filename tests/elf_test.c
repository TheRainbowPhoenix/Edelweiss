#include "../exword/elf.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "../sdk/edl.h"

static unsigned char f[8192];
static unsigned file_size=256;
static unsigned char memory[65536];
static unsigned writes;
static void w16(unsigned n, unsigned v) { f[n]=v>>8; f[n+1]=v; }
static void w32(unsigned n, uint32_t v) { w16(n,v>>16); w16(n+2,v); }
static int read_at(void *ctx, uint32_t off, void *dst, size_t n)
{
    (void)ctx;
    if(off > file_size || n > file_size-off) return -1;
    memcpy(dst, f+off, n); return 0;
}
static void fixture(void)
{
    file_size=256;
    memset(f,0,sizeof f); memcpy(f,"\177ELF\1\2\1",7);
    w16(16,2); w16(18,42); w32(20,1); w32(24,0x8c200000);
    w32(28,52); w16(40,52); w16(42,32); w16(44,1);
    w32(52,1); w32(56,128); w32(60,0x8c200000);
    w32(68,16); w32(72,32); w32(76,5); w32(80,4);
}
static int inspect(void)
{
    struct edl_image image;
    return edl_elf_inspect(read_at,0,file_size,0x8c200000,0x8c210000,&image);
}
static int write_at(void *ctx,uint32_t address,const void *data,size_t n)
{
    (void)ctx;
    assert(address>=0x8c200000 && address<0x8c210000);
    assert(n<=0x8c210000-address);
    writes++; memcpy(memory+address-0x8c200000,data,n); return 0;
}
static int failed_read(void *ctx,uint32_t off,void *dst,size_t n)
{ return off>=128 ? -1 : read_at(ctx,off,dst,n); }
int main(void)
{
    fixture(); assert(inspect()==0);
    fixture(); f[5]=1; assert(inspect()<0);
    fixture(); w16(18,3); assert(inspect()<0);
    fixture(); w32(68,33); assert(inspect()<0);
    fixture(); w32(56,250); assert(inspect()<0);
    fixture(); w32(60,0x8c20fff0); assert(inspect()<0);
    fixture(); w32(24,0x8c200010); assert(inspect()<0);
    fixture(); w32(24,0x8c200001); assert(inspect()<0);
    fixture(); w32(76,4); assert(inspect()<0);
    fixture(); w32(80,3); assert(inspect()<0);
    fixture(); w16(44,2); memcpy(f+84,f+52,32); assert(inspect()<0);
    fixture(); w32(28,0xfffffff0); assert(inspect()<0);
    fixture(); w32(52,2); assert(inspect()<0);
    fixture(); w32(52,3); assert(inspect()<0);
    fixture(); w32(52,7); assert(inspect()<0);
    struct edl_image image;
    fixture(); memset(f+128,0x5a,16); memset(memory,0xa5,sizeof memory);
    assert(!edl_elf_load(read_at,0,file_size,0x8c200000,0x8c210000,write_at,0,&image));
    assert(memory[0]==0x5a && memory[15]==0x5a);
    assert(memory[16]==0 && memory[31]==0 && memory[32]==0xa5);
    fixture(); w16(44,2); memcpy(f+84,f+52,32); writes=0;
    assert(edl_elf_load(read_at,0,file_size,0x8c200000,0x8c210000,write_at,0,&image)<0);
    assert(writes==0); /* Validate every segment before touching destination. */
    fixture(); writes=0;
    assert(edl_elf_load(failed_read,0,file_size,0x8c200000,0x8c210000,write_at,0,&image)<0);
    assert(writes==0);
    /* A valid non-allocated metadata section and bounded string table. */
    fixture(); file_size=sizeof f;
    w32(32,256); w16(46,40); w16(48,3); w16(50,1);
    w32(256+40+4,3); w32(256+40+16,400); w32(256+40+20,32);
    memcpy(f+400,"\0.shstrtab\0.edl.meta\0",21);
    w32(256+80,11); w32(256+80+4,1);
    w32(256+80+16,512); w32(256+80+20,sizeof(struct edl_metadata));
    w32(512,EDL_ABI_MAGIC); w32(516,1); memcpy(f+520,"Test",5);
    struct edl_metadata meta;
    assert(!edl_elf_metadata(read_at,0,file_size,&meta,sizeof meta));
    assert(!strcmp(meta.title,"Test"));
    w32(256+80+8,2); /* Metadata must not be an allocated executable section. */
    assert(edl_elf_metadata(read_at,0,file_size,&meta,sizeof meta)<0);
    puts("ELF validation checks passed"); return 0;
}
