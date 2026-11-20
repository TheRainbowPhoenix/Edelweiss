#ifndef EDELWEISS_ELF_H
#define EDELWEISS_ELF_H
#include <stdint.h>
#include <stddef.h>

/* ELF32, big-endian SH ET_EXEC. Addresses are fixed by the linker; the caller
   owns the whole permitted address range and must keep it owned until return. */
#define EDL_MAX_SEGMENTS 16
struct edl_segment {
    uint32_t offset, address, file_size, memory_size, flags;
};
struct edl_image {
    uint32_t entry;
    unsigned count;
    struct edl_segment segments[EDL_MAX_SEGMENTS];
};
typedef int (*edl_read_fn)(void *context, uint32_t offset, void *buffer,
    size_t size);
/* Validate completely before copying. end is exclusive. Returns 0 or -1.
   The read callback must return 0 only when all requested bytes were read. */
int edl_elf_inspect(edl_read_fn read, void *context, uint32_t file_size,
    uint32_t start, uint32_t end, struct edl_image *image);
/* Locate exactly one SDK metadata section; no ELF addresses are dereferenced. */
int edl_elf_metadata(edl_read_fn read, void *context, uint32_t file_size,
    void *metadata, size_t metadata_size);
typedef int (*edl_write_fn)(void *context, uint32_t address,
    const void *data, size_t size);
/* Inspect all headers first; then copy PT_LOAD data and zero BSS. */
int edl_elf_load(edl_read_fn read, void *read_context, uint32_t file_size,
    uint32_t start, uint32_t end, edl_write_fn write, void *write_context,
    struct edl_image *image);
/* SDK --emit-relocs ELFs can move as a whole between firmware arenas.
   Old fixed ELFs remain usable only at their original address. */
int edl_elf_load_at(edl_read_fn read, void *read_context, uint32_t file_size,
    uint32_t link_start, uint32_t link_end, uint32_t actual_start,
    edl_write_fn write, void *write_context, struct edl_image *image);
#endif
