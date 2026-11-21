#include "host.h"
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/drivers/touch.h>
#include <gint/gint.h>
#include <gint/rtc.h>
#include <string.h>
#include <stdio.h>

static char active[EDL_PATH_MAX], next[EDL_PATH_MAX], boot[EDL_PATH_MAX];
static void rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0 || w > DWIDTH || h > DHEIGHT)
        return;
    if (x < 0 || y < 0 || x > DWIDTH - w || y > DHEIGHT - h)
        return;
    drect(x, y, x + w - 1, y + h - 1, color);
}
static void text(int x, int y, uint16_t color, const char *s)
{
    dtext(x, y, color, s);
}
static void blit(int x, int y, int w, int h, const uint16_t *pixels)
{
    if (w <= 0 || h <= 0 || w > DWIDTH || h > DHEIGHT || x < 0 || y < 0 || x > DWIDTH - w ||
        y > DHEIGHT - h)
        return;
    uint16_t *vram;
    dgetvram(&vram, NULL);
    for (int row = 0; row < h; row++)
        memcpy(vram + (y + row) * DWIDTH + x, pixels + row * w, (size_t)w * 2);
}
static void present(void)
{
    dupdate();
}
static int wait_timeout(struct edl_event *out, unsigned timeout_ms)
{
    if (timeout_ms > 60000)
        timeout_ms = 60000;
    uint32_t start = timeout_ms ? rtc_ticks() : 0;
    uint32_t ticks = (timeout_ms * 128u + 999u) / 1000u;
    for (;;)
    {
        key_event_t e = pollevent();
        *out = (struct edl_event){0};
        if (timeout_ms)
        {
            uint32_t now = rtc_ticks();
            uint32_t elapsed = now >= start ? now - start : 128u * 86400u - start + now;
            if (elapsed >= ticks && e.type == KEYEV_NONE)
                return 0;
        }
        if (e.type == KEYEV_TOUCH_DOWN)
        {
            out->key = EDL_TOUCH;
            out->x = e.x;
            out->y = e.y;
            return 0;
        }
        if (e.type != KEYEV_DOWN && e.type != KEYEV_HOLD)
            continue;
        /* Only navigation repeats; activation and exit require a fresh press. */
        if (e.type == KEYEV_HOLD && e.key != KEY_UP && e.key != KEY_DOWN && e.key != KEY_LEFT &&
            e.key != KEY_RIGHT)
            continue;
        switch (e.key)
        {
        case KEY_UP:
            out->key = EDL_UP;
            break;
        case KEY_DOWN:
            out->key = EDL_DOWN;
            break;
        case KEY_LEFT:
            out->key = EDL_LEFT;
            break;
        case KEY_RIGHT:
            out->key = EDL_RIGHT;
            break;
        case KEY_EXE:
            out->key = EDL_OK;
            break;
        case KEY_EXIT:
        case KEY_ACON:
            out->key = EDL_BACK;
            break;
        case KEY_MENU:
            out->key = EDL_HOME;
            break;
        case KEY_EX_R:
        case KEY_F5:
            out->key = EDL_REFRESH;
            break;
        default:
            continue;
        }
        return 0;
    }
}
static int wait_event(struct edl_event *out)
{
    return wait_timeout(out, 0);
}

static int status(struct edl_status *out)
{
    static int battery = -1;
    static uint32_t sampled = UINT32_MAX;
    rtc_time_t t;
    rtc_get_time(&t);
    /* gint's implementation returns the hardware RMONCNT value (1..12). */
    *out = (struct edl_status){.year = t.year,
                               .month = t.month,
                               .day = t.month_day,
                               .hours = t.hours,
                               .minutes = t.minutes,
                               .seconds = t.seconds,
                               .battery = -1};
    if (t.year < 1900 || t.year > 9999 || t.month < 1 || t.month > 12 || t.month_day < 1 ||
        t.month_day > 31 || t.hours > 23 || t.minutes > 59 || t.seconds > 59)
        return -1;
    uint32_t now = t.hours * 3600u + t.minutes * 60u + t.seconds;
    if (sampled == UINT32_MAX || now < sampled || now - sampled >= 30)
    {
        battery = edl_battery_state();
        sampled = now;
    }
    out->battery = (int8_t)battery;
    return 0;
}
static int request_launch(const char *path)
{
    size_t n = 0;
    while (n < sizeof next && path[n])
        n++;
    if (n >= sizeof next || n < 3 || path[0] != '\\' || path[1] != '\\' || !edl_has_extension(path))
        return -1;
    memcpy(next, path, n + 1);
    return 0;
}
static void loading(void)
{
    dclear(C_WHITE);
    dtext(DWIDTH - 82, DHEIGHT - 16, 0xbdf7, "loading...");
    dupdate();
}
/* Discard the launch/exit gesture through its release. Clearing the queue
   alone leaves held keys and the pen active in the next application. */
static int release_input(void)
{
    /* A stale pen-down scan or a stuck key must not spin forever while the
       screen still says "loading...". Normal input release is instantaneous.
       Abort launch after two seconds or if the RTC stops ticking. */
    uint32_t start = rtc_ticks();
    for (unsigned spins = 0; spins < 8000000u; spins++)
    {
        struct exword_touch_debug touch;
        clearevents();
        touch_exword_get_last_scan(&touch);
        if (!keydown_any(KEY_EXE, KEY_EXIT, KEY_ACON, KEY_MENU, 0) && !touch.pen_down)
        {
            clearevents();
            return 0;
        }
        uint32_t now = rtc_ticks();
        uint32_t elapsed = now >= start ? now - start : 128u * 86400u - start + now;
        if (elapsed >= 256u)
            break;
    }
    clearevents();
    return -1;
}
static void message(const char *title, const char *detail, int rc)
{
    dclear(C_WHITE);
    dtext(20, 20, C_BLACK, title);
    dtext(20, 50, C_BLACK, detail);
    dprint(20, 80, C_BLACK, "Result %d   ELF arena %08x..%08x", rc,
           (unsigned)edl_active_arena.arena_start, (unsigned)edl_active_arena.arena_end);
    dtext(20, 110, C_BLACK, "Press a key to continue.");
    dupdate();
    struct edl_event e;
    wait_event(&e);
}
static struct edl_api api = {
    .magic = EDL_ABI_MAGIC,
    .version = EDL_ABI_VERSION,
    .size = sizeof api,
    .width = DWIDTH,
    .height = DHEIGHT,
    .image_path = active,
    .rect = rect,
    .text = text,
    .blit = blit,
    .present = present,
    .wait = wait_event,
    .scan = edl_scan,
    .metadata = edl_metadata,
    .read = edl_read,
    .os_call = edl_os_call,
    .launch = request_launch,
    .status = status,
    .wait_timeout = wait_timeout,
};

int main(void)
{
    loading();
    int profile_rc = edl_profile_init();
    if (profile_rc)
    {
        message(profile_rc == -10 ? "Edelweiss: unsupported firmware"
                                  : "Edelweiss: RAM check failed",
                profile_rc == -10 ? "No compatible syscall AE buffer or legacy profile."
                                  : "Extended payload RAM is occupied or mirrored.",
                profile_rc);
        touch_exword_quit();
        return -2;
    }
    api.arena_start = edl_active_arena.arena_start;
    api.arena_end = edl_active_arena.arena_end;
    api.stack_start = edl_active_arena.stack_start;
    api.stack_end = edl_active_arena.stack_end;
    api.syscall_dispatcher = *(volatile uint32_t *)0x74000000;
    if (edl_boot_path(boot))
    {
        message("Edelweiss: payload missing",
                "Place main.edl or EBrew.edl beside this D01 in _CONTENT.", -11);
        edl_profile_release();
        touch_exword_quit();
        return -2;
    }
    memcpy(active, boot, sizeof active);
    const char *leaf = strrchr(boot, '\\');
    int browser = leaf && edl_name_equal(leaf + 1, "ebrew.edl");
    for (;;)
    {
        /* Evict old operand aliases before reusing the physical arena. Do
           not invalidate the entire operand cache or touch OS TLB state. */
        for (uint32_t p = api.arena_start; p < api.stack_end; p += 32)
            __asm__ volatile("ocbp @%0" ::"r"(p) : "memory");
        __asm__ volatile("synco" ::: "memory");
        loading();
        struct edl_image image;
        next[0] = 0;
        int rc = edl_load(active, &image);
        if (!rc)
        {
            edl_sync_code();
            *(volatile uint32_t *)(uintptr_t)api.stack_start = 0x45444c53;
            if (release_input())
            {
                message("Edelweiss: input held",
                        "Release the launch key or touch screen and retry.", -21);
                break;
            }
            rc = edl_invoke(image.entry, &api, api.stack_end);
            edl_gint_cleanup();
            if (*(volatile uint32_t *)(uintptr_t)api.stack_start != 0x45444c53)
            {
                message("Edelweiss: stack overflow", active, -12);
                break;
            }
            if (release_input())
            {
                message("Edelweiss: input held", "Key or pen held after exit.", -21);
                break;
            }
            if (rc == EDL_LAUNCH && next[0])
            {
                memcpy(active, next, sizeof active);
                continue;
            }
        }
        if (rc < 0 || (rc == EDL_LAUNCH && !next[0]))
            message("Edelweiss: payload failed", active, rc);
        if (!browser || !strcmp(active, boot))
            break;
        memcpy(active, boot, sizeof active);
    }
    edl_profile_release();
    touch_exword_quit();
    return -2;
}
