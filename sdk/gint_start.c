#include "edl.h"
#include <gint/kmalloc.h>
#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>

#ifndef EDL_HEAP_SIZE
#define EDL_HEAP_SIZE (256 * 1024)
#endif
static unsigned char heap[EDL_HEAP_SIZE] __attribute__((aligned(32)));
static kmalloc_arena_t arena;
extern void (*edl_ctors_start[])(void), (*edl_ctors_end[])(void);
extern void (*edl_dtors_start[])(void), (*edl_dtors_end[])(void);
int main(void);
extern void edl_gint_cleanup(void);
static jmp_buf return_point;
static int exit_code, finalizing;
_Noreturn void _Exit(int code)
{
    exit_code = code;
    edl_gint_cleanup();
    longjmp(return_point, 1);
}
_Noreturn void abort(void)
{
    _Exit(-2);
}
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
    if (!setjmp(return_point))
    {
        for (void (**p)(void) = edl_ctors_start; p < edl_ctors_end; p++)
            (*p)();
        exit_code = main();
    }
    edl_gint_cleanup();
    if (!finalizing)
    {
        finalizing = 1;
        for (void (**p)(void) = edl_dtors_end; p > edl_dtors_start;)
            (*--p)();
    }
    kmalloc_remove_arena(&arena);
    return exit_code;
}
