#include "host.h"
#include "memory.h"
#include "read_cache.h"
#include <gint/exword.h>
#include <gint/gint.h>
#include <string.h>
#include <limits.h>

/* Only one ELF/metadata file is processed at a time in the native
   world. Put the 8 KiB read cache in D01 BSS, not on the OS call stack. */
static struct edl_read_cache elf_cache;

struct io
{
    const char *path;
    uint32_t offset, size;
    void *buffer;
    struct edl_memory *mapping;
};
static int read_exact(void *ctx, uint32_t off, void *dst, size_t n)
{
    int fd = *(int *)ctx;
    if (off > INT_MAX || n > INT_MAX || exword_sys_seek(fd, (int)off, 0) < 0)
        return -1;
    unsigned char *p = dst;
    while (n)
    {
        int rc = exword_sys_read(fd, p, (int)n);
        if (rc <= 0 || (size_t)rc > n)
            return -1;
        p += rc;
        n -= (size_t)rc;
    }
    return 0;
}
static int read_native(struct io *q)
{
    int fd = exword_sys_open(q->path, 1);
    if (fd < 0)
        return fd;
    int size = exword_sys_get_filesize(fd), rc = -1;
    if (size >= 0 && q->offset <= (uint32_t)size && q->size <= (uint32_t)size - q->offset)
        rc = read_exact(&fd, q->offset, q->buffer, q->size);
    int close = exword_sys_close(fd);
    return rc < 0 ? rc : close < 0 ? close : (int)q->size;
}
int edl_read(const char *path, uint32_t offset, void *dst, uint32_t size)
{
    if (size > INT_MAX)
        return -1;
    struct io q = {.path = path, .offset = offset, .size = size, .buffer = dst};
    return gint_world_switch(GINT_CALL(read_native, (void *)&q));
}
static int metadata_native(struct io *q)
{
    int fd = exword_sys_open(q->path, 1);
    if (fd < 0)
        return fd;
    int size = exword_sys_get_filesize(fd);
    int rc = size;
    if (size >= 0)
    {
        edl_read_cache_init(&elf_cache, read_exact, &fd, (uint32_t)size);
        rc = edl_elf_metadata(edl_read_cache_read, &elf_cache, (uint32_t)size, q->buffer,
                              sizeof(struct edl_metadata));
    }
    int close = exword_sys_close(fd);
    if (rc < 0)
        return rc;
    struct edl_metadata *m = q->buffer;
    m->title[31] = 0;
    m->description[63] = 0;
    return close < 0 ? close : 0;
}
int edl_metadata(const char *path, struct edl_metadata *out)
{
    struct io q = {.path = path, .buffer = out};
    return gint_world_switch(GINT_CALL(metadata_native, (void *)&q));
}
static int write_physical(void *ctx, uint32_t address, const void *data, size_t size)
{
    (void)ctx;
    memcpy((void *)(uintptr_t)(address | 0x20000000u), data, size);
    return 0;
}
static int load_native(struct io *q)
{
    int fd = exword_sys_open(q->path, 1);
    if (fd < 0)
        return fd;
    int size = exword_sys_get_filesize(fd);
    int rc;
    if (size < 0)
        rc = size;
    else
    {
        edl_read_cache_init(&elf_cache, read_exact, &fd, (uint32_t)size);
        if (q->mapping)
            rc = edl_elf_load_to(edl_read_cache_read, &elf_cache, (uint32_t)size, EDL_MEMORY_VBASE,
                                 EDL_MEMORY_IMAGE_END, edl_memory_write, q->mapping,
                                 edl_resolve_gint, NULL, q->buffer);
        else
            rc = edl_elf_load_auto(edl_read_cache_read, &elf_cache, (uint32_t)size,
                                   edl_active_arena.arena_start, edl_active_arena.arena_end,
                                   write_physical, NULL, edl_resolve_gint, NULL, q->buffer);
    }
    int close = exword_sys_close(fd);
    return rc < 0 ? rc : close < 0 ? close : 0;
}
int edl_load(const char *path, struct edl_image *image)
{
    struct io q = {.path = path, .buffer = image};
    return gint_world_switch(GINT_CALL(load_native, (void *)&q));
}
int edl_load_mapped(const char *path, struct edl_memory *mapping, struct edl_image *image)
{
    if (!mapping || !mapping->allocation || mapping->poisoned || mapping->mapped)
        return -1;
    struct io q = {.path = path, .buffer = image, .mapping = mapping};
    int rc = gint_world_switch(GINT_CALL(load_native, (void *)&q));
    return rc < 0 ? rc : edl_memory_finish_load(mapping);
}

/* Resolve uploaded names using directory entries: internal storage can be
   case-sensitive and installers may lowercase both files and directories. */
struct child
{
    const char *parent, *name;
    char *out;
    int directory;
};
static int child_native(struct child *q)
{
    char pattern[EDL_PATH_MAX], name[512] = {0};
    if (edl_join(pattern, q->parent, "*"))
        return -1;
    int handle = -1;
    unsigned long kind = 0;
    int rc = exword_sys_findfirst(pattern, &handle, name, &kind), found = -1;
    if (rc < 0)
        return rc;
    do
    {
        name[sizeof name - 1] = 0;
        if ((kind == 5) == q->directory && edl_name_equal(name, q->name))
        {
            found = edl_join(q->out, q->parent, name);
            break;
        }
        name[0] = 0;
        kind = 0;
        rc = exword_sys_findnext(handle, name, &kind);
    } while (rc >= 0);
    int close = exword_sys_findclose(handle);
    return close < 0 ? close : found;
}
static int child(const char *parent, const char *name, int directory, char *out)
{
    struct child q = {parent, name, out, directory};
    return gint_world_switch(GINT_CALL(child_native, (void *)&q));
}

static int root_native(struct child *q)
{
    const char *sep = strrchr(q->parent + 2, '\\');
    if (!sep)
    {
        size_t n = strlen(q->parent);
        if (n >= EDL_PATH_MAX)
            return -1;
        memcpy(q->out, q->parent, n + 1);
        return 0;
    }
    char parent[EDL_PATH_MAX];
    size_t n = (size_t)(sep - q->parent);
    if (n >= sizeof parent)
        return -1;
    memcpy(parent, q->parent, n);
    parent[n] = 0;
    struct child sub = {parent, sep + 1, q->out, 1};
    return child_native(&sub);
}
static int root_path(const char *root, char *out)
{
    struct child q = {.parent = root, .out = out};
    return gint_world_switch(GINT_CALL(root_native, (void *)&q));
}

struct scan
{
    unsigned first, capacity, total;
    struct edl_record *rows;
    int error;
};
static int done(int rc)
{
    return rc == -1 || rc == -16;
}
static void scan_content(struct scan *q, const char *directory)
{
    char pattern[EDL_PATH_MAX], name[512] = {0};
    if (edl_join(pattern, directory, "*"))
        return;
    int handle = -1;
    unsigned long kind = 0;
    int rc = exword_sys_findfirst(pattern, &handle, name, &kind);
    if (rc < 0)
    {
        if (!done(rc))
            q->error = rc;
        return;
    }
    do
    {
        name[sizeof name - 1] = 0;
        if (kind != 5 && edl_has_extension(name))
        {
            char full[EDL_PATH_MAX];
            if (!edl_join(full, directory, name))
            {
                if (q->total >= q->first && q->total - q->first < q->capacity)
                {
                    struct edl_record *r = q->rows + (q->total - q->first);
                    memcpy(r->path, full, strlen(full) + 1);
                    size_t n = strlen(name) - 4;
                    if (n >= sizeof r->title)
                        n = sizeof r->title - 1;
                    memcpy(r->title, name, n);
                    r->title[n] = 0;
                }
                q->total++;
            }
        }
        name[0] = 0;
        kind = 0;
        rc = exword_sys_findnext(handle, name, &kind);
    } while (rc >= 0);
    int close = exword_sys_findclose(handle);
    if (!done(rc))
        q->error = rc;
    if (close < 0)
        q->error = close;
}
static void scan_root(struct scan *q, const char *root)
{
    char pattern[EDL_PATH_MAX], name[512] = {0};
    if (edl_join(pattern, root, "*"))
        return;
    int handle = -1;
    unsigned long kind = 0;
    int rc = exword_sys_findfirst(pattern, &handle, name, &kind);
    /* An unmounted optional card or absent CASIO root is normal. */
    if (rc < 0)
        return;
    do
    {
        name[sizeof name - 1] = 0;
        if (kind == 5 && strcmp(name, ".") && strcmp(name, ".."))
        {
            char app[EDL_PATH_MAX], content[EDL_PATH_MAX];
            if (!edl_join(app, root, name))
            {
                struct child sub = {app, "_CONTENT", content, 1};
                if (!child_native(&sub))
                    scan_content(q, content);
            }
        }
        name[0] = 0;
        kind = 0;
        rc = exword_sys_findnext(handle, name, &kind);
    } while (rc >= 0);
    int close = exword_sys_findclose(handle);
    if (!done(rc))
        q->error = rc;
    if (close < 0)
        q->error = close;
}
static int scan_native(struct scan *q)
{
    static const char *roots[] = {"\\\\drv0", "\\\\drv0\\CASIO", "\\\\crd0\\CASIO",
                                  "\\\\crd1\\CASIO"};
    for (unsigned i = 0; i < sizeof roots / sizeof *roots; i++)
    {
        char root[EDL_PATH_MAX];
        struct child sub = {.parent = roots[i], .out = root};
        if (!root_native(&sub))
            scan_root(q, root);
    }
    /* Keep a partial successful catalog usable when one device fails. */
    return q->total ? (int)q->total : q->error;
}
int edl_scan(unsigned first, struct edl_record *rows, unsigned capacity)
{
    if (capacity && !rows)
        return -1;
    struct scan q = {.first = first, .capacity = capacity, .rows = rows};
    return gint_world_switch(GINT_CALL(scan_native, (void *)&q));
}

struct call
{
    uint32_t id, a, b, c, d;
};
static int call_native(struct call *q)
{
    return edl_native_dispatch(q->id, q->a, q->b, q->c, q->d);
}

/* Directory enumerator used by GUI apps needing read-only navigation.
   Like EXWord-Explorer, keep the native search handle alive for the
   *entire* native enumeration, never across a pair of world switches. */
struct bulk_list {
    const char *directory;
    unsigned first, capacity;
    struct edl_dirent *rows;
};
static int bulk_list_native(struct bulk_list *q)
{
    char pattern[EDL_PATH_MAX], name[512]={0};
    size_t n=strlen(q->directory);
    if(!n || n+3>=sizeof pattern)return -1;
    memcpy(pattern,q->directory,n);
    if(pattern[n-1]!='\\')pattern[n++]='\\';
    pattern[n++]='*';pattern[n]=0;
    unsigned long kind=0;
    int handle=-1;
    int rc=exword_sys_findfirst(pattern,&handle,name,&kind);
    if(rc<0)return rc==-16 || rc==-1 ? 0 : rc;
    unsigned total=0;
    do {
        name[sizeof name-1]=0;
        size_t len=strlen(name);
        if(len && len<sizeof q->rows[0].name &&
           strcmp(name,".") && strcmp(name,"..")) {
            if(total>=q->first && total-q->first<q->capacity) {
                struct edl_dirent *row=&q->rows[total-q->first];
                memcpy(row->name,name,len+1);
                row->directory=kind==5;
            }
            total++;
        }
        name[0]=0;kind=0;
        /* The native filesystem can be scanned in bounded windows when
           the caller knows the total from the initial enumeration. */
        rc=exword_sys_findnext(handle,name,&kind);
    }while(rc>=0);
    int closed=exword_sys_findclose(handle);
    if(closed<0)return closed;
    if(rc!=-1 && rc!=-16)return -1;
    return (int)total;
}

/* Keep open/get_filesize/close within a single native OS-world switch. */
struct native_size { const char *path; };
static int native_file_size(struct native_size *q)
{
    int fd=exword_sys_open(q->path,1);
    if(fd<0)return -1;
    int n=exword_sys_get_filesize(fd);
    int closed=exword_sys_close(fd);
    return n>=0 && closed>=0?n:-1;
}

int edl_os_call(uint32_t id, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    if(id==EDL_OS_FILE_SIZE) {
        if(!a)return -1;
        struct native_size q={.path=(const char *)(uintptr_t)a};
        return gint_world_switch(GINT_CALL(native_file_size, (void *)&q));
    }
    if(id==EDL_OS_BULK_LIST) {
        struct bulk_list q={
            .directory=(const char *)(uintptr_t)a,
            .first=b,.rows=(struct edl_dirent *)(uintptr_t)c,.capacity=d
        };
        if(!q.directory || !q.rows || q.capacity>8 || q.first>65535)
            return -1;
        return gint_world_switch(GINT_CALL(bulk_list_native, (void *)&q));
    }
    struct call q = {id, a, b, c, d};
    return gint_world_switch(GINT_CALL(call_native, (void *)&q));
}

static int exists(const char *path)
{
    unsigned char magic[7];
    return edl_read(path, 0, magic, sizeof magic) == sizeof magic &&
           !memcmp(magic, "\177ELF\1\2\1", sizeof magic);
}
int edl_boot_path_for_id(char out[EDL_PATH_MAX], const char id[8])
{
    /* ROM32A syscall 0x15 copies the active 8-byte content ID and metadata.
       The ID is d0<folder>, c0<folder> or c1<folder> on D6600.
       Older colon-prefixed IDs are also accepted. Other firmware can fall back to
       locating the configured package ID, including on optional crd1. */
    char appid[7] = {0};
    unsigned order[4];
    if (edl_content_id(id, appid, order))
        memcpy(appid, EDL_APPID, sizeof EDL_APPID);
    const char *roots[4] = {"\\\\drv0", "\\\\drv0\\CASIO", "\\\\crd0\\CASIO", "\\\\crd1\\CASIO"};
    for (unsigned i = 0; i < 4; i++)
    {
        char root[EDL_PATH_MAX], app[EDL_PATH_MAX], content[EDL_PATH_MAX];
        if (root_path(roots[order[i]], root) || child(root, appid, 1, app) ||
            child(app, "_CONTENT", 1, content))
            continue;
        if (!child(content, "main.edl", 0, out) && exists(out))
            return 0;
        if (!child(content, "ebrew.edl", 0, out) && exists(out))
            return 0;
    }
    return -1;
}

int edl_boot_path(char out[EDL_PATH_MAX])
{
    char id[9] = {0}, unused[8];
    edl_os_call(0x15, (uint32_t)(uintptr_t)id, (uint32_t)(uintptr_t)unused, 0, 0);
    return edl_boot_path_for_id(out, id);
}
