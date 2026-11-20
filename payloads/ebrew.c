#include "edl.h"
#include "ebrew_assets.h"

static const struct edl_api *host;
static struct edl_record rows[12];
static struct edl_metadata metadata;
static int total,selected,page,error;
static const uint16_t ink=0x3186,blue=0x3d9b,white=0xffff;

static void label(int x,int y,uint16_t color,const char *s)
{ host->text(x,y,color,s); }
static void box(int x,int y,int w,int h,uint16_t c)
{ host->rect(x,y,w,h,c); }
static void line(int x0,int y0,int x1,int y1,uint16_t c)
{
    int dx=x1-x0,dy=y1-y0;
    int steps=dx<0 ? -dx : dx, yy=dy<0 ? -dy : dy;
    if(yy>steps) steps=yy;
    if(!steps) { box(x0,y0,1,1,c); return; }
    for(int i=0;i<=steps;i++) box(x0+dx*i/steps,y0+dy*i/steps,1,1,c);
}
static void number(char *dst,unsigned n)
{
    char tmp[12]; unsigned len=0;
    do { tmp[len++]=(char)('0'+n%10); n/=10; } while(n);
    for(unsigned i=0;i<len;i++) dst[i]=tmp[len-1-i];
    dst[len]=0;
}
static void short_title(char dst[13],const char *src)
{
    unsigned i=0;
    while(i<12 && src[i]) { unsigned char c=src[i]; dst[i]=(c>=32 && c<127)?(char)c:'?'; i++; }
    dst[i]=0;
}
static void refresh(void)
{
    /* A card may disappear between reads. Bound retries and never display
       stale records from a page that no longer exists. */
    for(int attempt=0;attempt<3;attempt++) {
        page=selected/12;
        total=host->scan((unsigned)page*12,rows,12);
        error=total<0 ? total : 0;
        if(total<0) total=0;
        if(selected>=total) selected=total ? total-1 : 0;
        if(page==selected/12) return;
    }
    total=selected=page=0; error=-1;
}
static void header(void)
{
    box(0,0,528,320,0xf7be);
    host->blit(0,20,528,160,edl_wallpaper);
    box(0,0,528,20,0x8c51);
    label(10,5,white,"EDELWEISS"); label(375,5,white,"XD-A8500 / EDL");
    /* NDS-inspired analog dial and a compact calendar-like tile panel. */
    box(16,30,126,82,0xbdd7); box(18,32,122,78,white);
    for(int i=0;i<5;i++) {
        box(24+i*24,36,1,70,0xe71c); box(24,36+i*16,110,1,0xe71c);
    }
    label(74,36,0x9cd3,"12"); label(76,93,0x9cd3,"6");
    label(24,65,0x9cd3,"9"); label(125,65,0x9cd3,"3");
    line(80,72,80,44,ink); line(80,72,116,61,blue); box(78,70,5,5,ink);
    label(166,37,ink,"Edelweiss Homebrew");
    label(166,57,0x6b4d,"Physical ELF runtime");
    label(166,77,0x6b4d,"Internal + SD slot 1 + SD slot 2");
    label(166,97,error ? 0xf800 : blue,error ?
        "Storage unavailable. Press R to rescan." : "Tap to select; tap again to launch.");
    box(12,122,504,53,0x8c51); box(14,124,500,49,0x18e3);
    label(57,134,white,"No programs found");
    label(57,152,0xbdd7,"Install .edl files inside an add-in's _CONTENT folder.");
}
static void draw(void)
{
    header();
    if(total) {
        int index=selected-page*12;
        const struct edl_record *r=&rows[index];
        box(14,124,500,49,0x18e3);
        if(!host->metadata(r->path,&metadata)) {
            host->blit(20,132,32,32,metadata.icon);
            label(62,134,white,metadata.title);
            label(62,152,0xbdd7,metadata.description);
        } else {
            label(24,134,white,r->title);
            label(24,152,0xbdd7,"ELF application");
        }
    }
    for(int i=0;i<12;i++) {
        int x=12+(i%6)*86,y=186+(i/6)*54;
        int current=page*12+i,focus=current==selected && current<total;
        box(x,y,76,49,focus ? blue : 0xc638);
        box(x+2,y+2,72,45,white);
        if(current>=total) {
            box(x+29,y+12,18,18,0xef7d);
            continue;
        }
        const char *name=rows[i].title;
        if(!host->metadata(rows[i].path,&metadata)) {
            host->blit(x+22,y+3,32,32,metadata.icon); name=metadata.title;
        } else {
            box(x+24,y+5,28,28,0x9e99);
            label(x+30,y+14,white,"E");
        }
        char title[13]; short_title(title,name);
        label(x+3,y+36,focus ? blue : ink,title);
    }
    box(0,296,528,24,0x8c51);
    label(12,304,white,"EXE: Open   R: Rescan   Back: Exit");
    char n[12]; number(n,(unsigned)(total ? selected+1 : 0));
    label(426,304,white,n); label(453,304,white,"/");
    number(n,(unsigned)total); label(469,304,white,n);
    host->present();
}
int edl_main(const struct edl_api *api)
{
    host=api; refresh();
    for(;;) {
        draw();
        struct edl_event e;
        if(host->wait(&e)<0) return -1;
        int launch=0,old=selected;
        if(e.key==EDL_BACK || e.key==EDL_HOME) return EDL_EXIT;
        if(e.key==EDL_REFRESH) { refresh(); continue; }
        if(e.key==EDL_LEFT) selected--;
        if(e.key==EDL_RIGHT) selected++;
        if(e.key==EDL_UP) selected-=6;
        if(e.key==EDL_DOWN) selected+=6;
        if(e.key==EDL_OK) launch=1;
        if(e.key==EDL_TOUCH && e.x>=12 && e.y>=186 && e.y<289) {
            int col=(e.x-12)/86,row=(e.y-186)/54;
            if(col<6 && row<2 && (e.x-12)%86<76 && (e.y-186)%54<49) {
                int choice=page*12+row*6+col;
                if(choice<total) { launch=choice==selected; selected=choice; }
            }
        }
        if(total) {
            if(selected<0) selected=total-1;
            if(selected>=total) selected=0;
        } else selected=0;
        if(page!=selected/12) refresh();
        if(launch && total && selected==old) {
            if(!host->launch(rows[selected-page*12].path)) return EDL_LAUNCH;
        }
    }
}

EDL_METADATA const struct edl_metadata edl_meta={
    .magic=EDL_ABI_MAGIC,.version=1,.title="EBrew",
    .description="The Edelweiss home screen",
    .icon={ EDL_EBREW_ICON }
};
