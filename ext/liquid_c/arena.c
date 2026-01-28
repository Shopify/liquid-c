#include "arena.h"
#include <string.h>

/* Align size up to ARENA_ALIGNMENT boundary */
static inline size_t align_up(size_t size)
{
    return (size + ARENA_ALIGNMENT - 1) & ~(ARENA_ALIGNMENT - 1);
}

/* Allocate a new arena block */
static arena_block_t *arena_alloc_block(size_t min_size, size_t default_size)
{
    size_t block_size = min_size > default_size ? min_size : default_size;
    arena_block_t *block = xmalloc(sizeof(arena_block_t) + block_size);
    block->next = NULL;
    block->size = block_size;
    block->used = 0;
    return block;
}

void *arena_alloc(arena_t *arena, size_t size)
{
    size_t aligned_size = align_up(size);

    /* Check if current block has space */
    if (arena->current != NULL) {
        size_t remaining = arena->current->size - arena->current->used;
        if (aligned_size <= remaining) {
            void *ptr = arena->current->data + arena->current->used;
            arena->current->used += aligned_size;
            arena->total_allocated += aligned_size;
            return ptr;
        }
    }

    /* Need a new block */
    arena_block_t *new_block = arena_alloc_block(aligned_size, arena->default_block_size);

    if (arena->current != NULL) {
        arena->current->next = new_block;
    } else {
        arena->first = new_block;
    }
    arena->current = new_block;

    void *ptr = new_block->data;
    new_block->used = aligned_size;
    arena->total_allocated += aligned_size;
    return ptr;
}

void *arena_calloc(arena_t *arena, size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = arena_alloc(arena, total);
    memset(ptr, 0, total);
    return ptr;
}

const char *arena_strdup(arena_t *arena, const char *str, size_t len)
{
    char *copy = arena_alloc(arena, len);
    memcpy(copy, str, len);
    return copy;
}

const char *arena_strndup(arena_t *arena, const char *str, size_t len)
{
    char *copy = arena_alloc(arena, len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

void arena_free(arena_t *arena)
{
    arena_block_t *block = arena->first;
    while (block != NULL) {
        arena_block_t *next = block->next;
        xfree(block);
        block = next;
    }
    arena->first = NULL;
    arena->current = NULL;
    arena->total_allocated = 0;
}

void arena_reset(arena_t *arena)
{
    /* Free all blocks except the first */
    if (arena->first != NULL) {
        arena_block_t *block = arena->first->next;
        while (block != NULL) {
            arena_block_t *next = block->next;
            xfree(block);
            block = next;
        }
        arena->first->next = NULL;
        arena->first->used = 0;
        arena->current = arena->first;
    }
    arena->total_allocated = 0;
}

size_t arena_total_capacity(const arena_t *arena)
{
    size_t total = 0;
    arena_block_t *block = arena->first;
    while (block != NULL) {
        total += block->size;
        block = block->next;
    }
    return total;
}
