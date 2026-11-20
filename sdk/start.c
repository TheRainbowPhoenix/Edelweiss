#include "edl.h"
typedef void (*init_fn)(void);
extern init_fn edl_ctors_start[], edl_ctors_end[];
extern init_fn edl_dtors_start[], edl_dtors_end[];

__attribute__((section(".text.edl_entry")))
int edl_entry(const struct edl_api *api)
{
    if(!api || api->magic != EDL_ABI_MAGIC || api->version != EDL_ABI_VERSION ||
        api->size < sizeof *api) return -1;
    for(init_fn *p=edl_ctors_start; p<edl_ctors_end; p++) (*p)();
    int rc=edl_main(api);
    for(init_fn *p=edl_dtors_end; p>edl_dtors_start;) (*--p)();
    return rc;
}
