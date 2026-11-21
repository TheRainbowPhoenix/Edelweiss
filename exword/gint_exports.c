#include "host.h"
#include <gint/gint.h>
#include <gint/hardware.h>
#include <gint/cpu.h>
#include <gint/clock.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <gint/keyboard.h>
#include <gint/dma.h>
#include <gint/drivers/r61523.h>
#include <gint/drivers/touch.h>
#include <gint/exword.h>
#include <string.h>

static unsigned owned_timers;
static int owned_rtc;
static int payload_timer_configure(int timer, uint64_t delay, gint_call_t callback)
{
    int id = timer_configure(timer, delay, callback);
    if (id >= 0 && id < 9)
        owned_timers |= 1u << id;
    return id;
}
static bool payload_rtc_enable(int frequency, gint_call_t callback)
{
    bool ok = rtc_periodic_enable(frequency, callback);
    if (ok)
        owned_rtc = 1;
    return ok;
}
struct binding
{
    const char *name;
    uintptr_t address;
};
static const struct binding bindings[] = {
#define EDL_GINT_SYMBOL(name) {"_" #name, (uintptr_t)&name},
#include "../sdk/gint_imports.def"
#undef EDL_GINT_SYMBOL
};
int edl_resolve_gint(void *ctx, const char *name, uint32_t *address)
{
    (void)ctx;
    if (!strcmp(name, "_timer_configure"))
    {
        *address = (uintptr_t)payload_timer_configure;
        return 0;
    }
    if (!strcmp(name, "_rtc_periodic_enable"))
    {
        *address = (uintptr_t)payload_rtc_enable;
        return 0;
    }
    for (unsigned i = 0; i < sizeof bindings / sizeof *bindings; i++)
        if (!strcmp(bindings[i].name, name))
        {
            *address = (uint32_t)bindings[i].address;
            return 0;
        }
    return -1;
}
void edl_gint_cleanup(void)
{
    for (unsigned i = 0; i < 9; i++)
        if (owned_timers & (1u << i))
            timer_stop(i);
    owned_timers = 0;
    if (owned_rtc)
        rtc_periodic_disable();
    owned_rtc = 0;
    getkey_set_feature_function(NULL);
}
