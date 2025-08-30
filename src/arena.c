#include "arena.h"
#include <stdlib.h>
#include <string.h>

static size_t align_up(size_t x, size_t a) {
    size_t m = a - 1;
    return (x + m) & ~m;
}

TopoArena *arena_create(size_t cap) {
    TopoArena *A = (TopoArena *) malloc(sizeof(TopoArena));
    A->base = (uint8_t *) malloc(cap);
    A->cap = cap;
    A->off = 0;
    return A;
}

void *arena_alloc(TopoArena *A, size_t sz, size_t align) {
    size_t off = align_up(A->off, align);
    if (off + sz > A->cap) return NULL;
    void *p = A->base + off;
    A->off = off + sz;
    return p;
}

void arena_reset(TopoArena *A) {
    A->off = 0;
}

void arena_destroy(TopoArena *A) {
    if (A) {
        free(A->base);
        free(A);
    }
}
