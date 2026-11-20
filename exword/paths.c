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
