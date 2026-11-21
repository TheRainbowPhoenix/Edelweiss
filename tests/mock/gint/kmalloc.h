#ifndef MOCK_KMALLOC_H
#define MOCK_KMALLOC_H
#include <stdbool.h>
typedef struct
{
    const char *name;
    void *start, *end;
    int is_default;
} kmalloc_arena_t;
void kmalloc_init(void);
void kmalloc_init_arena(kmalloc_arena_t *arena, bool statistics);
bool kmalloc_add_arena(kmalloc_arena_t *arena);
void kmalloc_remove_arena(kmalloc_arena_t *arena);
#endif
