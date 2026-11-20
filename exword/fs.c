#include "host.h"
#include <gint/exword.h>
#include <gint/gint.h>
#include <string.h>
#include <limits.h>

struct io { const char *path; uint32_t offset,size; void *buffer; };
static int read_exact(void *ctx,uint32_t off,void *dst,size_t n)
{
    int fd=*(int *)ctx;
    if(off>INT_MAX || n>INT_MAX || exword_sys_seek(fd,(int)off,0)<0) return -1;
    unsigned char *p=dst;
    while(n) {
        int rc=exword_sys_read(fd,p,(int)n);
        if(rc<=0 || (size_t)rc>n) return -1;
        p+=rc; n-=(size_t)rc;
    }
    return 0;
}
static int read_native(struct io *q)
{
    int fd=exword_sys_open(q->path,1);
    if(fd<0) return fd;
    int size=exword_sys_get_filesize(fd),rc=-1;
    if(size>=0 && q->offset<=(uint32_t)size && q->size<=(uint32_t)size-q->offset)
        rc=read_exact(&fd,q->offset,q->buffer,q->size);
    int close=exword_sys_close(fd);
    return rc<0 ? rc : close<0 ? close : (int)q->size;
}
int edl_read(const char *path,uint32_t offset,void *dst,uint32_t size)
{
    if(size>INT_MAX) return -1;
    struct io q={path,offset,size,dst};
    return gint_world_switch(GINT_CALL(read_native,(void *)&q));
}
static int metadata_native(struct io *q)
{
    int fd=exword_sys_open(q->path,1);
    if(fd<0) return fd;
    int size=exword_sys_get_filesize(fd);
    int rc=size<0 ? size : edl_elf_metadata(read_exact,&fd,(uint32_t)size,
        q->buffer,sizeof(struct edl_metadata));
    int close=exword_sys_close(fd);
    if(rc<0) return rc;
    struct edl_metadata *m=q->buffer;
    m->title[31]=0; m->description[63]=0;
    return close<0 ? close : 0;
}
int edl_metadata(const char *path,struct edl_metadata *out)
{
    struct io q={.path=path,.buffer=out};
    return gint_world_switch(GINT_CALL(metadata_native,(void *)&q));
}
static int write_physical(void *ctx,uint32_t address,const void *data,size_t size)
{
    (void)ctx;
    memcpy((void *)(uintptr_t)(address|0x20000000u),data,size);
    return 0;
}
static int load_native(struct io *q)
{
    int fd=exword_sys_open(q->path,1);
    if(fd<0) return fd;
    int size=exword_sys_get_filesize(fd);
    int rc=size<0 ? size : edl_elf_load_at(read_exact,&fd,(uint32_t)size,
        EDL_ARENA_START,EDL_ARENA_END,edl_current->arena_start,write_physical,NULL,q->buffer);
    int close=exword_sys_close(fd);
    return rc<0 ? rc : close<0 ? close : 0;
}
int edl_load(const char *path,struct edl_image *image)
{
    struct io q={.path=path,.buffer=image};
    return gint_world_switch(GINT_CALL(load_native,(void *)&q));
}

struct scan { unsigned first,capacity,total; struct edl_record *rows; int error; };
static int done(int rc) { return rc==-1 || rc==-16; }
static void scan_content(struct scan *q,const char *directory)
{
    char pattern[EDL_PATH_MAX],name[512]={0};
    if(edl_join(pattern,directory,"*")) return;
    int handle=-1; unsigned long kind=0;
    int rc=exword_sys_findfirst(pattern,&handle,name,&kind);
    if(rc<0) { if(!done(rc)) q->error=rc; return; }
    do {
        name[sizeof name-1]=0;
        if(kind!=5 && edl_has_extension(name)) {
            char full[EDL_PATH_MAX];
            if(!edl_join(full,directory,name)) {
                if(q->total>=q->first && q->total-q->first<q->capacity) {
                    struct edl_record *r=q->rows+(q->total-q->first);
                    memcpy(r->path,full,strlen(full)+1);
                    size_t n=strlen(name)-4;
                    if(n>=sizeof r->title) n=sizeof r->title-1;
                    memcpy(r->title,name,n); r->title[n]=0;
                }
                q->total++;
            }
        }
        name[0]=0; kind=0;
        rc=exword_sys_findnext(handle,name,&kind);
    } while(rc>=0);
    int close=exword_sys_findclose(handle);
    if(!done(rc)) q->error=rc;
    if(close<0) q->error=close;
}
static void scan_root(struct scan *q,const char *root)
{
    char pattern[EDL_PATH_MAX],name[512]={0};
    if(edl_join(pattern,root,"*")) return;
    int handle=-1; unsigned long kind=0;
    int rc=exword_sys_findfirst(pattern,&handle,name,&kind);
    /* An unmounted optional card or absent CASIO root is normal. */
    if(rc<0) return;
    do {
        name[sizeof name-1]=0;
        if(kind==5 && strcmp(name,".") && strcmp(name,"..")) {
            char app[EDL_PATH_MAX],content[EDL_PATH_MAX];
            if(!edl_join(app,root,name) && !edl_join(content,app,"_CONTENT"))
                scan_content(q,content);
        }
        name[0]=0; kind=0;
        rc=exword_sys_findnext(handle,name,&kind);
    } while(rc>=0);
    int close=exword_sys_findclose(handle);
    if(!done(rc)) q->error=rc;
    if(close<0) q->error=close;
}
static int scan_native(struct scan *q)
{
    static const char *roots[]={"\\\\drv0","\\\\drv0\\CASIO",
        "\\\\crd0\\CASIO","\\\\crd1\\CASIO"};
    for(unsigned i=0;i<sizeof roots/sizeof *roots;i++) scan_root(q,roots[i]);
    /* Keep a partial successful catalog usable when one device fails. */
    return q->total ? (int)q->total : q->error;
}
int edl_scan(unsigned first,struct edl_record *rows,unsigned capacity)
{
    if(capacity && !rows) return -1;
    struct scan q={.first=first,.capacity=capacity,.rows=rows};
    return gint_world_switch(GINT_CALL(scan_native,(void *)&q));
}

struct call { uint32_t id,a,b,c,d; };
static int call_native(struct call *q)
{ return edl_native_dispatch(q->id,q->a,q->b,q->c,q->d); }
int edl_os_call(uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    struct call q={id,a,b,c,d};
    return gint_world_switch(GINT_CALL(call_native,(void *)&q));
}

static int exists(const char *path)
{
    unsigned char magic[7];
    return edl_read(path,0,magic,sizeof magic)==sizeof magic &&
        !memcmp(magic,"\177ELF\1\2\1",sizeof magic);
}
int edl_boot_path(char out[EDL_PATH_MAX])
{
    char id[9]={0},unused[8];
    edl_os_call(0x15,(uint32_t)(uintptr_t)id,(uint32_t)(uintptr_t)unused,0,0);
    /* ROM32A syscall 0x15 copies the active 8-byte content ID and metadata.
       The ID is d0<folder>, c0<folder> or c1<folder> on D6600.
       Older colon-prefixed IDs are also accepted. Other firmware can fall back to
       locating the configured package ID, including on optional crd1. */
    char appid[7]={0};
    size_t n=0;
    if(id[0]=='d' || id[0]=='c') {
        while(n<6 && id[n+2]) { appid[n]=id[n+2]; n++; }
        for(size_t i=0;i<n;i++) if(appid[i]>='a' && appid[i]<='z') appid[i]-=32;
    }
    if(!n) memcpy(appid,EDL_APPID,sizeof EDL_APPID);
    const char *roots[4]={"\\\\drv0","\\\\drv0\\CASIO",
        "\\\\crd0\\CASIO","\\\\crd1\\CASIO"};
    unsigned order[4]={0,1,2,3};
    if(id[0]=='c') {
        order[0]=id[1]=='1' ? 3 : 2; order[1]=id[1]=='1' ? 2 : 3;
        order[2]=0; order[3]=1;
    }
    for(unsigned i=0;i<4;i++) {
        char app[EDL_PATH_MAX],content[EDL_PATH_MAX];
        if(edl_join(app,roots[order[i]],appid) || edl_join(content,app,"_CONTENT")) continue;
        if(!edl_join(out,content,"main.edl") && exists(out)) return 0;
        if(!edl_join(out,content,"EBrew.edl") && exists(out)) return 0;
    }
    return -1;
}
