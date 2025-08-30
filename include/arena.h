#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct TopoArena {
    uint8_t *base;
    size_t cap;
    size_t off;
} TopoArena;

TopoArena *arena_create(size_t cap);

void *arena_alloc(TopoArena *A, size_t sz, size_t align);

void arena_reset(TopoArena *A);

void arena_destroy(TopoArena *A);

#endif
