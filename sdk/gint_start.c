#include "edl.h"
#include <gint/kmalloc.h>
#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdint.h>

#ifndef EDL_HEAP_SIZE
#define EDL_HEAP_SIZE (256 * 1024)
#endif
static unsigned char heap[EDL_HEAP_SIZE] __attribute__((aligned(32)));
static kmalloc_arena_t arena;
extern void (*edl_ctors_start[])(void), (*edl_ctors_end[])(void);
extern void (*edl_dtors_start[])(void), (*edl_dtors_end[])(void);
int main(void);
__attribute__((used, section(".edl.gint"))) const uint32_t edl_gint_marker[] = {0x47494e54, 1};
__attribute__((section(".text.edl_entry"))) int edl_entry(const struct edl_api *api)
{
    if (!api || api->magic != EDL_ABI_MAGIC || api->version != EDL_ABI_VERSION ||
        api->size < sizeof *api)
        return -1;
    arena = (kmalloc_arena_t){
        .name = "edelweiss", .start = heap, .end = heap + sizeof heap, .is_default = 1};
    kmalloc_init();
    kmalloc_init_arena(&arena, false);
    if (!kmalloc_add_arena(&arena))
        return -1;
    dwindow_set((struct dwindow){0, 0, api->width, api->height});
    clearevents();
    for (void (**p)(void) = edl_ctors_start; p < edl_ctors_end; p++)
        (*p)();
    int rc = main();
    for (void (**p)(void) = edl_dtors_end; p > edl_dtors_start;)
        (*--p)();
    kmalloc_remove_arena(&arena);
    return rc;
}
