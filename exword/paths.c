#include "host.h"
#include <string.h>

int edl_join(char out[EDL_PATH_MAX],const char *parent,const char *name)
{
    if(!parent || !name || !*name || !strcmp(name,".") || !strcmp(name,"..")) return -1;
    for(size_t i=0;name[i];i++) {
        unsigned char c=(unsigned char)name[i];
        if((c>=0x81 && c<=0x9f) || (c>=0xe0 && c<=0xfc)) {
            if(!name[++i]) return -1;
            continue;
        }
        if(c=='\\' || c=='/') return -1;
    }
    size_t n=strlen(parent),m=strlen(name);
    if(n+1+m>=EDL_PATH_MAX) return -1;
    memcpy(out,parent,n); out[n++]='\\'; memcpy(out+n,name,m+1);
    return 0;
}

int edl_has_extension(const char *name)
{
    size_t n=strlen(name);
    if(n<5) return 0;
    const unsigned char *p=(const unsigned char *)name+n-4;
    return p[0]=='.' && (p[1]|32)=='e' && (p[2]|32)=='d' && (p[3]|32)=='l';
}

/* Native content IDs fit in eight bytes and need not be NUL-terminated. */
int edl_content_id(const char id[8],char appid[7],unsigned order[4])
{
    order[0]=0; order[1]=1; order[2]=2; order[3]=3;
    appid[0]=0;
    if((id[0]!='d' && id[0]!='c') ||
        (id[1]!='0' && id[1]!='1' && id[1]!=':')) return -1;
    unsigned n=0;
    for(;n<6 && id[n+2];n++) {
        unsigned char c=(unsigned char)id[n+2];
        if(c>='a' && c<='z') c-=32;
        if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_')) {
            appid[0]=0; return -1;
        }
        appid[n]=(char)c;
    }
    appid[n]=0;
    if(!n) return -1;
    if(id[0]=='c') {
        order[0]=id[1]=='1' ? 3 : 2; order[1]=id[1]=='1' ? 2 : 3;
        order[2]=0; order[3]=1;
    }
    return 0;
}
