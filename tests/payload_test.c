/* Run the real EBrew UI against a host API, exercising launch/return rather
   than target hardware. Each process starts with fresh payload BSS. */
#include "../sdk/edl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int event_index,scenario,scans,presents;
static char launched[EDL_PATH_MAX];
static void rect(int x,int y,int w,int h,uint16_t c)
{ (void)c; assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=528 && y+h<=320); }
static void text(int x,int y,uint16_t c,const char *s)
{ (void)c; assert(x>=0 && x<528 && y>=0 && y<320 && s); }
static void blit(int x,int y,int w,int h,const uint16_t *data)
{ assert(data); rect(x,y,w,h,0); }
static void present(void) { presents++; }
static int scan(unsigned first,struct edl_record *rows,unsigned capacity)
{
    scans++;
    int count=scenario==2 && scans>=2 ? 0 : 15;
    for(unsigned i=0;i<capacity && first+i<(unsigned)count;i++) {
        snprintf(rows[i].path,sizeof rows[i].path,"\\\\crd1\\CASIO\\APP01\\_CONTENT\\app%u.edl",first+i);
        snprintf(rows[i].title,sizeof rows[i].title,"App %u",first+i);
    }
    return count;
}
static int metadata(const char *path,struct edl_metadata *out)
{
    (void)path;
    memset(out,0,sizeof *out); strcpy(out->title,"Test application");
    strcpy(out->description,"Host UI interaction test"); return 0;
}
static int launch(const char *path) { strcpy(launched,path); return 0; }
static int wait_event(struct edl_event *e)
{
    *e=(struct edl_event){0};
    if(scenario==0) e->key=event_index++<12 ? EDL_RIGHT : EDL_OK;
    if(scenario==1) {
        e->key=EDL_TOUCH; e->x=100; e->y=200; event_index++;
    }
    if(scenario==2) e->key=event_index++ ? EDL_HOME : EDL_REFRESH;
    assert(event_index<20); return 0;
}
int main(int argc,char **argv)
{
    scenario=argc>1 ? atoi(argv[1]) : 0;
    struct edl_api api={.magic=EDL_ABI_MAGIC,.version=1,.size=sizeof api,
        .width=528,.height=320,.rect=rect,.text=text,.blit=blit,
        .present=present,.scan=scan,.metadata=metadata,.launch=launch,.wait=wait_event};
    int rc=edl_main(&api);
    if(scenario==0) {
        assert(rc==EDL_LAUNCH && strstr(launched,"app12.edl"));
        assert(scans==2);
    }
    if(scenario==1) {
        assert(rc==EDL_LAUNCH && strstr(launched,"app1.edl"));
        assert(event_index==2); /* First tap selects; the second launches. */
    }
    if(scenario==2) assert(rc==EDL_EXIT && !launched[0]);
    assert(presents>0);
    printf("EBrew scenario %d passed\n",scenario);
}
