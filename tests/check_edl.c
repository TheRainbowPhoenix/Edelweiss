/* Run the device's actual parser and copy/zero implementation against CI ELFs. */
#include "../exword/elf.h"
#include "../sdk/edl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define BASE 0x8c700000u
#define END 0x8c7f0000u
static unsigned char memory[END-BASE],original[END-BASE];
static uint32_t actual=BASE;
static int read_file(void *ctx,uint32_t at,void *dst,size_t n)
{
    FILE *f=ctx;
    return fseek(f,(long)at,SEEK_SET) || fread(dst,1,n,f)!=n ? -1 : 0;
}
static int write_memory(void *ctx,uint32_t address,const void *src,size_t n)
{
    (void)ctx;
    assert(address>=actual && address<actual+sizeof memory && n<=actual+sizeof memory-address);
    memcpy(memory+address-actual,src,n); return 0;
}
int main(int argc,char **argv)
{
    assert(argc>1);
    for(int a=1;a<argc;a++) {
        FILE *f=fopen(argv[a],"rb"); assert(f);
        assert(!fseek(f,0,SEEK_END)); long size=ftell(f);
        assert(size>0 && size<0x1000000);
        memset(memory,0xa5,sizeof memory);
        struct edl_image image;
        assert(!edl_elf_load(read_file,f,(uint32_t)size,BASE,END,write_memory,0,&image));
        memcpy(original,memory,sizeof original);
        actual=0x8c800000;
        memset(memory,0xa5,sizeof memory);
        assert(!edl_elf_load_at(read_file,f,(uint32_t)size,BASE,END,actual,write_memory,0,&image));
        assert(image.entry>=actual && image.entry<actual+sizeof memory);
        unsigned changed=0;
        for(unsigned k=0;k<sizeof memory;k+=4) {
            uint32_t before=(uint32_t)original[k]<<24 | (uint32_t)original[k+1]<<16 |
                (uint32_t)original[k+2]<<8 | original[k+3];
            uint32_t after=(uint32_t)memory[k]<<24 | (uint32_t)memory[k+1]<<16 |
                (uint32_t)memory[k+2]<<8 | memory[k+3];
            if(before!=after) {
                assert(before>=BASE && before<=END && after==before+actual-BASE);
                changed++;
            }
        }
        assert(changed>10); /* Actual code/data pointers moved, not just headers. */
        unsigned loaded=0;
        for(unsigned i=0;i<image.count;i++) {
            struct edl_segment *s=&image.segments[i];
            loaded+=s->file_size;
            for(uint32_t j=s->file_size;j<s->memory_size;j++)
                assert(memory[s->address-actual+j]==0);
        }
        assert(loaded>131072); /* Real loadable bytes, not debug/ELF padding. */
        struct edl_metadata meta;
        assert(!edl_elf_metadata(read_file,f,(uint32_t)size,&meta,sizeof meta));
        assert(meta.title[0] && memchr(meta.title,0,sizeof meta.title));
        printf("%s: %u loadable bytes, title %s, entry %08x\n",argv[a],loaded,meta.title,image.entry);
        printf("  D6600 relocated pointers: %u\n",changed);
        actual=BASE;
        fclose(f);
    }
}
