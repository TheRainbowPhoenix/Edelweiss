#include "host.h"
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/drivers/touch.h>
#include <gint/gint.h>
#include <string.h>
#include <stdio.h>

static char active[EDL_PATH_MAX],next[EDL_PATH_MAX],boot[EDL_PATH_MAX];
static void rect(int x,int y,int w,int h,uint16_t color)
{
    if(w<=0 || h<=0 || w>DWIDTH || h>DHEIGHT) return;
    if(x<0 || y<0 || x>DWIDTH-w || y>DHEIGHT-h) return;
    drect(x,y,x+w-1,y+h-1,color);
}
static void text(int x,int y,uint16_t color,const char *s)
{ dtext(x,y,color,s); }
static void blit(int x,int y,int w,int h,const uint16_t *pixels)
{
    if(w<=0 || h<=0 || w>DWIDTH || h>DHEIGHT || x<0 || y<0 ||
        x>DWIDTH-w || y>DHEIGHT-h) return;
    uint16_t *vram;
    dgetvram(&vram,NULL);
    for(int row=0;row<h;row++)
        memcpy(vram+(y+row)*DWIDTH+x,pixels+row*w,(size_t)w*2);
}
static void present(void) { dupdate(); }
static int wait_event(struct edl_event *out)
{
    for(;;) {
        key_event_t e=waitevent(NULL);
        *out=(struct edl_event){0};
        if(e.type==KEYEV_TOUCH_DOWN) {
            out->key=EDL_TOUCH; out->x=e.x; out->y=e.y; return 0;
        }
        if(e.type!=KEYEV_DOWN && e.type!=KEYEV_HOLD) continue;
        switch(e.key) {
        case KEY_UP: out->key=EDL_UP; break;
        case KEY_DOWN: out->key=EDL_DOWN; break;
        case KEY_LEFT: out->key=EDL_LEFT; break;
        case KEY_RIGHT: out->key=EDL_RIGHT; break;
        case KEY_EXE: out->key=EDL_OK; break;
        case KEY_EXIT: case KEY_ACON: out->key=EDL_BACK; break;
        case KEY_MENU: out->key=EDL_HOME; break;
        case KEY_EX_R: case KEY_F5: out->key=EDL_REFRESH; break;
        default: continue;
        }
        return 0;
    }
}
static int request_launch(const char *path)
{
    size_t n=0;
    while(n<sizeof next && path[n]) n++;
    if(n>=sizeof next || n<3 || path[0]!='\\' || path[1]!='\\' ||
        !edl_has_extension(path)) return -1;
    memcpy(next,path,n+1);
    return 0;
}
static void message(const char *title,const char *detail,int rc)
{
    dclear(C_WHITE);
    dtext(20,20,C_BLACK,title);
    dtext(20,50,C_BLACK,detail);
    dprint(20,80,C_BLACK,"Result %d   ELF arena %08x..%08x",rc,
        (unsigned)EDL_ARENA_START,(unsigned)EDL_ARENA_END);
    dtext(20,110,C_BLACK,"Press a key to continue.");
    dupdate();
    struct edl_event e; wait_event(&e);
}
static struct edl_api api={
    .magic=EDL_ABI_MAGIC,.version=EDL_ABI_VERSION,.size=sizeof api,
    .width=DWIDTH,.height=DHEIGHT,.image_path=active,
    .rect=rect,.text=text,.blit=blit,.present=present,.wait=wait_event,
    .scan=edl_scan,.metadata=edl_metadata,.read=edl_read,
    .os_call=edl_os_call,.launch=request_launch,
    .arena_start=EDL_ARENA_START,.arena_end=EDL_ARENA_END,
    .stack_start=0x8c7f8000,.stack_end=0x8c800000,
};

int main(void)
{
    if(!edl_profile_supported()) {
        message("Edelweiss: unsupported firmware",
            "This build requires XD-A8500 ROM32A.", -10);
        return -2;
    }
    api.syscall_dispatcher=*(volatile uint32_t *)0x74000000;
    if(edl_boot_path(boot)) {
        message("Edelweiss: payload missing",
            "Place main.edl or EBrew.edl beside this D01 in _CONTENT.",-11);
        return -2;
    }
    memcpy(active,boot,sizeof active);
    const char *leaf=strrchr(boot,'\\');
    int browser=leaf && !strcmp(leaf+1,"EBrew.edl");
    for(;;) {
        /* Evict old operand aliases before reusing the physical arena. Do
           not invalidate the entire operand cache or touch OS TLB state. */
        for(uint32_t p=EDL_ARENA_START;p<0x8c800000;p+=32)
            __asm__ volatile("ocbp @%0" :: "r"(p) : "memory");
        __asm__ volatile("synco" ::: "memory");
        struct edl_image image;
        next[0]=0;
        int rc=edl_load(active,&image);
        if(!rc) {
            edl_sync_code();
            *(volatile uint32_t *)0x8c7f8000=0x45444c53;
            clearevents();
            rc=edl_invoke(image.entry,&api,api.stack_end);
            if(*(volatile uint32_t *)0x8c7f8000!=0x45444c53) {
                message("Edelweiss: stack overflow",active,-12);
                break;
            }
            clearevents();
            if(rc==EDL_LAUNCH && next[0]) {
                memcpy(active,next,sizeof active);
                continue;
            }
        }
        if(rc<0 || (rc==EDL_LAUNCH && !next[0]))
            message("Edelweiss: payload failed",active,rc);
        if(!browser || !strcmp(active,boot)) break;
        memcpy(active,boot,sizeof active);
    }
    touch_exword_quit();
    return -2;
}
