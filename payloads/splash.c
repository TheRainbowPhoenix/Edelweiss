#include "splash.h"
#include "ebrew_assets.h"

int ebrew_splash(const struct edl_api *api)
{
    uint16_t row[EDL_SPLASH_WIDTH];
    unsigned run=0,remaining=0;
    uint16_t color=0;
    for(unsigned y=0;y<EDL_SPLASH_HEIGHT;y++) {
        for(unsigned x=0;x<EDL_SPLASH_WIDTH;x++) {
            if(!remaining) {
                if(run>=edl_splash_runs) return -1;
                remaining=edl_splash_rle[run][0];
                color=edl_splash_rle[run++][1];
                if(!remaining) return -1;
            }
            row[x]=color; remaining--;
        }
        api->blit(0,(int)y,EDL_SPLASH_WIDTH,1,row);
    }
    if(remaining || run!=edl_splash_runs) return -1;
    api->present();
    return 0;
}
