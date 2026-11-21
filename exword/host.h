#ifndef EDELWEISS_HOST_H
#define EDELWEISS_HOST_H
#include "../sdk/edl.h"
#include "elf.h"
#include "firmware.h"
extern const struct edl_firmware *edl_current;

int edl_join(char out[EDL_PATH_MAX], const char *parent, const char *name);
int edl_content_id(const char id[8], char appid[7], unsigned order[4]);
int edl_name_equal(const char *a, const char *b);
int edl_has_extension(const char *name);
int edl_scan(unsigned first, struct edl_record *rows, unsigned capacity);
int edl_read(const char *path, uint32_t offset, void *dst, uint32_t size);
int edl_metadata(const char *path, struct edl_metadata *out);
int edl_load(const char *path, struct edl_image *image);
struct edl_memory;
int edl_load_mapped(const char *path, struct edl_memory *mapping, struct edl_image *image);
int edl_os_call(uint32_t id, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
int edl_native_dispatch(uint32_t id, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
int edl_invoke(uint32_t entry, const struct edl_api *api, uint32_t stack);
int edl_boot_path_for_id(char out[EDL_PATH_MAX], const char id[8]);
int edl_boot_path(char out[EDL_PATH_MAX]);
int edl_profile_init(void);
void edl_profile_release(void);
int edl_resolve_gint(void *ctx, const char *name, uint32_t *address);
void edl_gint_cleanup(void);
void edl_sync_code(void);
int edl_battery_state(void);
/* Firmware-gated, read-only diagnostic. Returned memory remains OS-owned. */
int edl_a8500_scratch_probe(struct edl_scratch_range *out);
#endif
