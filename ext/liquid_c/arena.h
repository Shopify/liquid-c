#ifndef LIQUID_ARENA_H
#define LIQUID_ARENA_H

#include <ruby.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Arena allocator for efficient AST node allocation.
 * Memory is allocated in large blocks and freed all at once.
 */

#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)  /* 64KB blocks */
#define ARENA_ALIGNMENT 8

/* Arena block for memory allocation */
typedef struct arena_block {
    struct arena_block *next;
    size_t size;
    size_t used;
    uint8_t data[];  /* Flexible array member */
} arena_block_t;

/* Arena allocator */
typedef struct arena {
    arena_block_t *current;
    arena_block_t *first;
    size_t default_block_size;
    size_t total_allocated;
} arena_t;

/* Initialize arena with default block size */
static inline void arena_init(arena_t *arena)
{
    arena->current = NULL;
    arena->first = NULL;
    arena->default_block_size = ARENA_DEFAULT_BLOCK_SIZE;
    arena->total_allocated = 0;
}

/* Initialize arena with custom block size */
static inline void arena_init_with_size(arena_t *arena, size_t block_size)
{
    arena_init(arena);
    arena->default_block_size = block_size;
}

/* Allocate memory from arena (aligned to ARENA_ALIGNMENT) */
void *arena_alloc(arena_t *arena, size_t size);

/* Allocate zeroed memory from arena */
void *arena_calloc(arena_t *arena, size_t count, size_t size);

/* Duplicate string into arena */
const char *arena_strdup(arena_t *arena, const char *str, size_t len);

/* Duplicate string into arena (null-terminated) */
const char *arena_strndup(arena_t *arena, const char *str, size_t len);

/* Free entire arena */
void arena_free(arena_t *arena);

/* Reset arena for reuse (keeps first block allocated) */
void arena_reset(arena_t *arena);

/* Get total bytes allocated */
static inline size_t arena_total_allocated(const arena_t *arena)
{
    return arena->total_allocated;
}

/* Get total capacity (block sizes) */
size_t arena_total_capacity(const arena_t *arena);

#endif /* LIQUID_ARENA_H */
