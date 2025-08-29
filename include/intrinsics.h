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
            QRing *data;
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

void qm_init(QMesh *m);

int qm_addv(QMesh *m, Vector3 v);

void qm_addq(QMesh *m, int a, int b, int c, int d);

void mesh_merge(QMesh *dst, const QMesh *src);

void mesh_mirror_x(QMesh *m, float weld);

void mesh_mirror_y(QMesh *m, float weld);

void mesh_mirror_z(QMesh *m, float weld);

void mesh_rotate_y(QMesh *m, float rad);

void mesh_rotate_x(QMesh *m, float rad);

void mesh_rotate_z(QMesh *m, float rad);

void mesh_move(QMesh *m, float dx, float dy, float dz);

void mesh_scale(QMesh *m, float sx, float sy, float sz);

void stitch(QMesh *m, const QRing *a, const QRing *b);

QRing ring_ellipse(QMesh *tmp, float cx, float cy, float rx, float ry, int segments);

QRing ring_grow_out(QMesh *tmp, const QRing *in, float step, float dz);

void ring_lift_x(QMesh *m, QRing *inout, float dx);

void ring_lift_y(QMesh *m, QRing *inout, float dy);

void ring_lift_z(QMesh *tmp, QRing *inout, float dz);

#endif