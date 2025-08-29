#ifndef INTRINSICS_H
#define INTRINSICS_H

#include <stddef.h>
#include "arena.h"
#include "mesh.h"

typedef struct {
    int k;
    union {
        double num;
        struct {
            char *s;
        } str;
        QMesh *mesh;
        QRing *ring;
        struct {
            QRing **ptrs;
            int count;
        } ringlist;
    };
} Value;
enum {
    VAL_VOID = 0, VAL_NUMBER, VAL_STRING, VAL_MESH, VAL_RING, VAL_RINGLIST
};
typedef struct Host {
    TopoArena *arena;
    QMesh *build;

    void *(*alloc)(struct Host *H, size_t sz, size_t align);
} Host;
typedef struct {
    const char *name;

    Value (*fn)(Host *, Value *, int, char err[256]);
} Builtin;

const Builtin *intrinsics_table(int *outCount);

#endif