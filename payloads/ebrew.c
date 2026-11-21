#include "edl.h"
#include "ebrew_assets.h"
#include "calendar.h"
#include "splash.h"
#include <string.h>

enum
{
    COLUMNS = 6,
    PAGE_SIZE = 18,
    GRID_Y = 134,
    ROW_STEP = 54,
    TILE_W = 76,
    TILE_H = 49
};
static const struct edl_api *host;
static struct edl_record rows[PAGE_SIZE];
static struct edl_metadata metadata;
static struct
{
    char title[32];
    uint16_t icon[32 * 32];
    int has_icon;
} tiles[PAGE_SIZE];
static struct edl_status now;
static int total, selected, page, error, date_valid;
static const uint16_t ink = 0x3186, blue = 0x3d9b, white = 0xffff;
static void label(int x, int y, uint16_t color, const char *s)
{
    host->text(x, y, color, s);
}
static void box(int x, int y, int w, int h, uint16_t c)
{
    host->rect(x, y, w, h, c);
}
static void line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int steps = dx < 0 ? -dx : dx, yy = dy < 0 ? -dy : dy;
    if (yy > steps)
        steps = yy;
    if (!steps)
    {
        box(x0, y0, 1, 1, c);
        return;
    }
    for (int i = 0; i <= steps; i++)
        box(x0 + dx * i / steps, y0 + dy * i / steps, 1, 1, c);
}
static void number(char *dst, unsigned n)
{
    char tmp[12];
    unsigned len = 0;
    do
    {
        tmp[len++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    for (unsigned i = 0; i < len; i++)
        dst[i] = tmp[len - 1 - i];
    dst[len] = 0;
}
static void two(char *s, unsigned n)
{
    s[0] = (char)('0' + n / 10);
    s[1] = (char)('0' + n % 10);
}
static void short_title(char dst[13], const char *src)
{
    unsigned i = 0;
    while (i < 12 && src[i])
    {
        unsigned char c = src[i];
        dst[i] = (c >= 32 && c < 127) ? (char)c : '?';
        i++;
    }
    dst[i] = 0;
}
static void update_status(void)
{
    now = (struct edl_status){.battery = -1};
    date_valid = host->status && !host->status(&now) && now.day >= 1 &&
                 now.day <= ebrew_month_days(now.year, now.month) && now.hours < 24 &&
                 now.minutes < 60 && now.seconds < 60;
}
static void cache_tiles(void)
{
    for (int i = 0; i < PAGE_SIZE; i++)
    {
        tiles[i].has_icon = 0;
        if (page * PAGE_SIZE + i >= total)
            continue;
        const char *name = rows[i].title;
        if (!host->metadata(rows[i].path, &metadata))
        {
            name = metadata.title;
            memcpy(tiles[i].icon, metadata.icon, sizeof tiles[i].icon);
            tiles[i].has_icon = 1;
        }
        unsigned n = 0;
        while (n < sizeof tiles[i].title - 1 && name[n])
        {
            tiles[i].title[n] = name[n];
            n++;
        }
        tiles[i].title[n] = 0;
    }
}
static void refresh(void)
{
    /* Present before scanning directories or reading icon metadata. */
    ebrew_splash(host);
    for (int attempt = 0; attempt < 3; attempt++)
    {
        page = selected / PAGE_SIZE;
        total = host->scan((unsigned)page * PAGE_SIZE, rows, PAGE_SIZE);
        error = total < 0 ? total : 0;
        if (total < 0)
            total = 0;
        if (selected >= total)
            selected = total ? total - 1 : 0;
        if (page == selected / PAGE_SIZE)
        {
            cache_tiles();
            return;
        }
    }
    total = selected = page = 0;
    error = -1;
}
static void battery(void)
{
    box(490, 5, 28, 11, white);
    box(518, 8, 3, 5, white);
    box(492, 7, 24, 7, 0x3186);
    if (now.battery == 1)
        box(493, 8, 22, 5, 0x47e8);
    else if (now.battery == 0)
        box(493, 8, 4, 5, 0xf800);
    else
        label(501, 5, white, "?");
}
static void clock_hand(unsigned step, int length, uint16_t color)
{
    static const int16_t sine[60] = {
        0,     105,  208,  309,  407,  500,  588,  669,  743,  809,  866,  914,  951,  978,  995,
        1000,  995,  978,  951,  914,  866,  809,  743,  669,  588,  500,  407,  309,  208,  105,
        0,     -105, -208, -309, -407, -500, -588, -669, -743, -809, -866, -914, -951, -978, -995,
        -1000, -995, -978, -951, -914, -866, -809, -743, -669, -588, -500, -407, -309, -208, -105};
    line(84, 76, 84 + sine[step % 60] * length / 1000, 76 - sine[(step + 15) % 60] * length / 1000,
         color);
}
static void clock_face(void)
{
    box(16, 26, 136, 98, 0xbdd7);
    box(18, 28, 132, 94, white);
    for (int x = 24; x < 148; x += 16)
        box(x, 29, 1, 92, 0xe71c);
    for (int y = 30; y < 122; y += 16)
        box(19, y, 130, 1, 0xe71c);
    label(78, 31, 0x9cd3, "12");
    label(82, 111, 0x9cd3, "6");
    label(23, 72, 0x9cd3, "9");
    label(140, 72, 0x9cd3, "3");
    if (date_valid)
    {
        clock_hand((now.hours % 12) * 5 + now.minutes / 12, 25, ink);
        clock_hand(now.minutes, 38, blue);
        clock_hand(now.seconds, 40, 0xc9a8);
        box(82, 74, 5, 5, ink);
    }
}
static void calendar(void)
{
    static const char *days[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    enum
    {
        X = 176,
        Y = 25,
        CELL = 48,
        DAY_Y = 51
    };
    box(X, Y, 336, 98, 0xbdd7);
    box(X + 1, Y + 1, 334, 12, white);
    char title[16] = "-- / ----";
    if (date_valid)
    {
        two(title, now.month);
        title[5] = (char)('0' + now.year / 1000);
        title[6] = (char)('0' + now.year / 100 % 10);
        title[7] = (char)('0' + now.year / 10 % 10);
        title[8] = (char)('0' + now.year % 10);
    }
    label(X + 141, Y + 2, ink, title);
    int first = date_valid ? ebrew_first_weekday(now.year, now.month) : -1;
    int count = date_valid ? ebrew_month_days(now.year, now.month) : 0;
    for (int col = 0; col < 7; col++)
    {
        int x = X + col * CELL;
        uint16_t color = col == 0 ? 0xe9d4 : col == 6 ? 0x32bd : ink;
        box(x + 1, Y + 14, CELL - 1, 12, color);
        label(x + 18, Y + 15, white, days[col]);
        for (int row = 0; row < 6; row++)
        {
            int y = DAY_Y + row * 12, day = row * 7 + col - first + 1;
            int valid = first >= 0 && day >= 1 && day <= count;
            int today = valid && day == now.day;
            box(x + 1, y + 1, CELL - 1, 11,
                today      ? ink
                : col == 0 ? 0xff5e
                : col == 6 ? 0xe77f
                           : white);
            if (valid)
            {
                char s[12];
                number(s, (unsigned)day);
                label(x + (day < 10 ? 21 : 18), y + 2,
                      today      ? white
                      : col == 0 ? 0xe800
                      : col == 6 ? 0x225f
                                 : ink,
                      s);
            }
        }
    }
}
static void header(void)
{
    box(0, 0, 528, 320, 0xf7be);
    host->blit(0, 20, 528, 160, edl_wallpaper);
    box(0, 0, 528, 20, 0x8c51);
    label(10, 5, white, "EDELWEISS");
    char time[6] = "--:--";
    if (date_valid)
    {
        two(time, now.hours);
        two(time + 3, now.minutes);
    }
    label(439, 5, white, time);
    battery();
    clock_face();
    calendar();
}
static void draw(void)
{
    header();
    for (int i = 0; i < PAGE_SIZE; i++)
    {
        int x = 12 + (i % COLUMNS) * 86, y = GRID_Y + (i / COLUMNS) * ROW_STEP;
        int current = page * PAGE_SIZE + i, focus = current == selected && current < total;
        box(x, y, TILE_W, TILE_H, focus ? blue : 0xc638);
        box(x + 2, y + 2, TILE_W - 4, TILE_H - 4, white);
        if (current >= total)
        {
            box(x + 29, y + 12, 18, 18, 0xef7d);
            continue;
        }
        if (tiles[i].has_icon)
            host->blit(x + 22, y + 3, 32, 32, tiles[i].icon);
        else
        {
            box(x + 24, y + 5, 28, 28, 0x9e99);
            label(x + 30, y + 14, white, "E");
        }
        char title[13];
        short_title(title, tiles[i].title);
        label(x + 3, y + 36, focus ? blue : ink, title);
    }
    box(0, 296, 528, 24, 0x8c51);
    label(12, 304, white,
          error    ? "Storage unavailable - R: Retry"
          : !total ? "No apps found - R: Rescan"
                   : "EXE: Open   R: Rescan   Back: Exit");
    char n[12];
    number(n, (unsigned)(total ? selected + 1 : 0));
    label(426, 304, white, n);
    label(453, 304, white, "/");
    number(n, (unsigned)total);
    label(469, 304, white, n);
    host->present();
}
int edl_main(const struct edl_api *api)
{
    host = api;
    refresh();
    update_status();
    for (;;)
    {
        draw();
        struct edl_event e;
        int rc = host->wait_timeout ? host->wait_timeout(&e, 1000) : host->wait(&e);
        if (rc < 0)
            return -1;
        update_status();
        int launch = 0, old = selected;
        if (e.key == EDL_BACK || e.key == EDL_HOME)
            return EDL_EXIT;
        if (e.key == EDL_REFRESH)
        {
            refresh();
            continue;
        }
        if (e.key == EDL_LEFT)
            selected--;
        if (e.key == EDL_RIGHT)
            selected++;
        if (e.key == EDL_UP)
            selected -= COLUMNS;
        if (e.key == EDL_DOWN)
            selected += COLUMNS;
        if (e.key == EDL_OK)
            launch = 1;
        if (e.key == EDL_TOUCH && e.x >= 12 && e.y >= GRID_Y && e.y < GRID_Y + 3 * ROW_STEP)
        {
            int col = (e.x - 12) / 86, row = (e.y - GRID_Y) / ROW_STEP;
            if (col < COLUMNS && row < 3 && (e.x - 12) % 86 < TILE_W &&
                (e.y - GRID_Y) % ROW_STEP < TILE_H)
            {
                int choice = page * PAGE_SIZE + row * COLUMNS + col;
                if (choice < total)
                {
                    launch = choice == selected;
                    selected = choice;
                }
            }
        }
        if (total)
        {
            if (selected < 0)
                selected = total - 1;
            if (selected >= total)
                selected = 0;
        }
        else
            selected = 0;
        if (page != selected / PAGE_SIZE)
            refresh();
        if (launch && total && selected == old)
        {
            if (!host->launch(rows[selected - page * PAGE_SIZE].path))
                return EDL_LAUNCH;
        }
    }
}
EDL_METADATA const struct edl_metadata edl_meta = {.magic = EDL_ABI_MAGIC,
                                                   .version = 1,
                                                   .title = "EBrew",
                                                   .description = "The Edelweiss home screen",
                                                   .icon = {EDL_EBREW_ICON}};
