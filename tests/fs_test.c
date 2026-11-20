#include "../exword/host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

const struct edl_firmware *edl_current;

struct directory { const char *pattern; const char *names[5]; unsigned kinds[5]; };
static const struct directory dirs[]={
    {"\\\\crd0\\*",{"CASIO",0},{5}},
    {"\\\\crd1\\*",{"CASIO",0},{5}},
    {"\\\\drv0\\*",{"EDELW","EXMPL",".","..",0},{5,5,5,5}},
    {"\\\\drv0\\EDELW\\*",{"_CONTENT",0},{5}},
    {"\\\\drv0\\EXMPL\\*",{"_CONTENT",0},{5}},
    {"\\\\crd0\\CASIO\\CARD0\\*",{"_CONTENT",0},{5}},
    {"\\\\crd1\\CASIO\\CARD1\\*",{"_CONTENT",0},{5}},
    {"\\\\drv0\\EDELW\\_CONTENT\\*",{"ebrew.edl","large-demo.edl",0},{1,1}},
    {"\\\\drv0\\EXMPL\\_CONTENT\\*",{"main.EDL","bad.edl","other.g3a",0},{1,5,1}},
    {"\\\\crd0\\CASIO\\*",{"CARD0",0},{5}},
    {"\\\\crd0\\CASIO\\CARD0\\_CONTENT\\*",{"app.edl",0},{1}},
    {"\\\\crd1\\CASIO\\*",{"CARD1",0},{5}},
    {"\\\\crd1\\CASIO\\CARD1\\_CONTENT\\*",{"second.EdL",0},{1}},
};
static int indexes[16],open_handles,opens,closes,missing_card,lowercase,bad_magic;
static unsigned file_offset;
static void actual(char *out,const char *in)
{
    strcpy(out,in);
    if(lowercase) for(char *p=out;*p;p++) if(*p>='A' && *p<='Z') *p+=32;
}
static int result(int handle,char *name,unsigned long *kind)
{
    const struct directory *d=&dirs[handle]; int i=indexes[handle]++;
    if(!d->names[i]) return -16;
    actual(name,d->names[i]); *kind=d->kinds[i]; return 0;
}
int exword_sys_findfirst(const char *p,int *handle,char *name,unsigned long *kind)
{
    if(missing_card && strstr(p,"crd1")) return -1;
    for(unsigned i=0;i<sizeof dirs/sizeof *dirs;i++) {
        char pattern[EDL_PATH_MAX]; actual(pattern,dirs[i].pattern);
        if(strcmp(p,pattern)) continue;
        *handle=(int)i; indexes[i]=0; open_handles++; opens++;
        return result((int)i,name,kind);
    }
    return -1;
}
int exword_sys_findnext(int h,char *name,unsigned long *kind) { return result(h,name,kind); }
int exword_sys_findclose(int h) { assert(h>=0); open_handles--; closes++; return 0; }
int exword_sys_open(const char *p,int mode)
{
    assert(mode==1);
    for(unsigned i=0;i<sizeof dirs/sizeof *dirs;i++) {
        char pattern[EDL_PATH_MAX]; actual(pattern,dirs[i].pattern);
        size_t n=strlen(pattern)-1;
        for(unsigned j=0;dirs[i].names[j];j++) if(dirs[i].kinds[j]!=5) {
            char name[512],path[EDL_PATH_MAX]; actual(name,dirs[i].names[j]);
            memcpy(path,pattern,n); strcpy(path+n,name);
            if(!strcmp(path,p)) { file_offset=0; return 99; }
        }
    }
    return -1;
}
int exword_sys_close(int h) { (void)h;return 0; }
int exword_sys_get_filesize(int h) { assert(h==99); return 7; }
int exword_sys_read(int h,void *p,int n)
{
    assert(h==99 && n>=0 && (unsigned)n<=7-file_offset);
    memcpy(p,(bad_magic ? "invalid" : "\177ELF\1\2\1")+file_offset,(unsigned)n);
    file_offset+=(unsigned)n; return n;
}
int exword_sys_seek(int h,int o,int w) { assert(h==99 && w==0 && o>=0 && o<=7); file_offset=(unsigned)o; return o; }
int edl_native_dispatch(uint32_t id,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ (void)id;(void)a;(void)b;(void)c;(void)d;return -1; }

int main(void)
{
    struct edl_record rows[8];
    assert(edl_scan(0,rows,8)==5);
    assert(!strcmp(rows[0].path,"\\\\drv0\\EDELW\\_CONTENT\\ebrew.edl"));
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
    char appid[7]; unsigned order[4];
    assert(!edl_content_id("d0edelw",appid,order) && !strcmp(appid,"EDELW") && order[0]==0);
    assert(!edl_content_id("c0exmpl",appid,order) && !strcmp(appid,"EXMPL") && order[0]==2);
    assert(!edl_content_id("c1exmpl",appid,order) && !strcmp(appid,"EXMPL") && order[0]==3);
    assert(!edl_content_id("c:edelw",appid,order) && order[0]==2);
    const char full_id[8]={'c','1','A','B','C','D','E','F'};
    assert(!edl_content_id(full_id,appid,order) && !strcmp(appid,"ABCDEF") && order[0]==3);
    assert(edl_content_id("c1../xx",appid,order)<0 && order[0]==0);
    const char empty_id[8]="c1";
    assert(edl_content_id(empty_id,appid,order)<0 && order[0]==0);
    assert(edl_content_id("z0edelw",appid,order)<0);
    assert(!edl_boot_path_for_id(path,"d0edelw"));
    assert(!strcmp(path,"\\\\drv0\\EDELW\\_CONTENT\\ebrew.edl"));
    assert(!edl_boot_path_for_id(path,"d0exmpl"));
    assert(!strcmp(path,"\\\\drv0\\EXMPL\\_CONTENT\\main.EDL"));
    lowercase=1;
    assert(!edl_boot_path_for_id(path,"d0edelw"));
    assert(!strcmp(path,"\\\\drv0\\edelw\\_content\\ebrew.edl"));
    assert(!edl_boot_path_for_id(path,"d0exmpl"));
    assert(!strcmp(path,"\\\\drv0\\exmpl\\_content\\main.edl"));
    assert(edl_scan(0,rows,8)==4);
    assert(!strcmp(rows[0].path,"\\\\drv0\\edelw\\_content\\ebrew.edl"));
    assert(edl_name_equal("EBrew.edl","ebrew.edl"));
    bad_magic=1;
    assert(edl_boot_path_for_id(path,"d0edelw")<0);
    assert(open_handles==0 && opens==closes);
    puts("Filesystem discovery, optional crd1, pagination and handle checks passed");
}
