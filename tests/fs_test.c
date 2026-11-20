#include "../exword/host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

const struct edl_firmware *edl_current;

struct directory { const char *pattern; const char *names[5]; unsigned kinds[5]; };
static const struct directory dirs[]={
    {"\\\\drv0\\*",{"EDELW","EXMPL",".","..",0},{5,5,5,5}},
    {"\\\\drv0\\EDELW\\_CONTENT\\*",{"EBrew.edl","large-demo.edl",0},{1,1}},
    {"\\\\drv0\\EXMPL\\_CONTENT\\*",{"main.EDL","bad.edl","other.g3a",0},{1,5,1}},
    {"\\\\crd0\\CASIO\\*",{"CARD0",0},{5}},
    {"\\\\crd0\\CASIO\\CARD0\\_CONTENT\\*",{"app.edl",0},{1}},
    {"\\\\crd1\\CASIO\\*",{"CARD1",0},{5}},
    {"\\\\crd1\\CASIO\\CARD1\\_CONTENT\\*",{"second.EdL",0},{1}},
};
static int indexes[16],open_handles,opens,closes,missing_card;
static int result(int handle,char *name,unsigned long *kind)
{
    const struct directory *d=&dirs[handle]; int i=indexes[handle]++;
    if(!d->names[i]) return -16;
    strcpy(name,d->names[i]); *kind=d->kinds[i]; return 0;
}
int exword_sys_findfirst(const char *p,int *handle,char *name,unsigned long *kind)
{
    if(missing_card && strstr(p,"crd1")) return -1;
    for(unsigned i=0;i<sizeof dirs/sizeof *dirs;i++) if(!strcmp(p,dirs[i].pattern)) {
        *handle=(int)i; indexes[i]=0; open_handles++; opens++;
        return result((int)i,name,kind);
    }
    return -1;
}
int exword_sys_findnext(int h,char *name,unsigned long *kind) { return result(h,name,kind); }
int exword_sys_findclose(int h) { assert(h>=0); open_handles--; closes++; return 0; }
int exword_sys_open(const char *p,int mode) { (void)p;(void)mode;return -1; }
int exword_sys_close(int h) { (void)h;return 0; }
int exword_sys_get_filesize(int h) { (void)h;return -1; }
int exword_sys_read(int h,void *p,int n) { (void)h;(void)p;(void)n;return -1; }
int exword_sys_seek(int h,int o,int w) { (void)h;(void)o;(void)w;return -1; }
int edl_native_dispatch(uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ (void)id;(void)a;(void)b;(void)c;(void)d;return -1; }

int main(void)
{
    struct edl_record rows[8];
    assert(edl_scan(0,rows,8)==5);
    assert(!strcmp(rows[0].path,"\\\\drv0\\EDELW\\_CONTENT\\EBrew.edl"));
    assert(!strcmp(rows[2].title,"main"));
    assert(!strcmp(rows[4].path,"\\\\crd1\\CASIO\\CARD1\\_CONTENT\\second.EdL"));
    assert(open_handles==0 && opens==closes);
    assert(edl_scan(3,rows,1)==5);
    assert(!strcmp(rows[0].path,"\\\\crd0\\CASIO\\CARD0\\_CONTENT\\app.edl"));
    missing_card=1;
    assert(edl_scan(0,rows,8)==4);
    assert(edl_scan(999,rows,8)==4);
    assert(edl_scan(0,0,0)==4);
    assert(edl_scan(0,0,1)<0);
    assert(open_handles==0 && opens==closes);
    char path[EDL_PATH_MAX];
    assert(edl_join(path,"\\\\drv0","..")<0);
    assert(edl_join(path,"\\\\drv0","bad/name")<0);
    assert(edl_join(path,"\\\\drv0","bad\\name")<0);
    assert(!edl_join(path,"\\\\drv0","\x81\x5c"));
    assert(!edl_has_extension(".edl"));
    assert(edl_has_extension("main.EdL"));
    puts("Filesystem discovery, optional crd1, pagination and handle checks passed");
}
