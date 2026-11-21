#include "../sdk/edl.h"
#include <gint/kmalloc.h>
#include <gint/display.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
int edl_test_entry(const struct edl_api *api);
_Noreturn void edl_test_Exit(int code);
_Noreturn void edl_test_abort(void);
_Noreturn void edl_test_exit(int code)
{
    edl_test_Exit(code);
}
static int mode, cleaned, ctors, dtors, removed, closed;
static void ctor(void)
{
    ctors++;
}
static void dtor(void)
{
    assert(cleaned);
    dtors++;
}
/* Model the exact linker-array bounds without relying on global ordering. */
void (*edl_ctors_start[])(void) = {ctor};
extern void (*edl_ctors_end[])(void);
void (*edl_dtors_start[])(void) = {dtor};
extern void (*edl_dtors_end[])(void);
#define STRING1(x) #x
#define STRING(x) STRING1(x)
#define LABEL(x) STRING(__USER_LABEL_PREFIX__) #x
__asm__(".global " LABEL(edl_ctors_end) "\n.set " LABEL(edl_ctors_end) ", " LABEL(edl_ctors_start) " + " STRING(
    __SIZEOF_POINTER__) "\n"
                        ".global " LABEL(edl_dtors_end) "\n.set " LABEL(edl_dtors_end) ", " LABEL(
                            edl_dtors_start) " + " STRING(__SIZEOF_POINTER__) "\n");
void kmalloc_init(void)
{
}
void kmalloc_init_arena(kmalloc_arena_t *a, bool stats)
{
    assert(!stats && (uintptr_t)a->end - (uintptr_t)a->start == 262144);
}
bool kmalloc_add_arena(kmalloc_arena_t *a)
{
    assert(a->is_default);
    return true;
}
void kmalloc_remove_arena(kmalloc_arena_t *a)
{
    (void)a;
    assert(closed);
    removed++;
}
struct dwindow dwindow_set(struct dwindow w)
{
    assert(w.right == 528 && w.bottom == 320);
    return w;
}
void clearevents(void)
{
}
void edl_gint_set_api(const struct edl_api *api)
{
    assert(api->magic == EDL_ABI_MAGIC);
}
void edl_gint_close_files(void)
{
    assert(dtors == 1);
    closed++;
}
void edl_gint_cleanup(void)
{
    cleaned++;
}
int payload_main(void)
{
    assert(ctors == 1 && !dtors);
    if (mode == 1)
        edl_test_Exit(23);
    if (mode == 2)
        edl_test_abort();
    return 17;
}
int main(void)
{
    const struct edl_api api = {.magic = EDL_ABI_MAGIC,
                                .version = EDL_ABI_VERSION,
                                .size = sizeof api,
                                .width = 528,
                                .height = 320};
    assert((uintptr_t)edl_ctors_end - (uintptr_t)edl_ctors_start == sizeof(void *));
    for (mode = 0; mode < 3; mode++)
    {
        cleaned = ctors = dtors = removed = closed = 0;
        int code = edl_test_entry(&api);
        assert(code == (mode == 0 ? 17 : mode == 1 ? 23 : -2));
        assert(ctors == 1 && dtors == 1 && removed == 1 && closed == 1);
    }
    assert(edl_test_entry(NULL) < 0);
    puts("Hosted CRT constructors, main return, exit, abort and cleanup ordering passed");
}
