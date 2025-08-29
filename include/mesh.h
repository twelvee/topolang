#ifndef MESH_H
#define MESH_H

#include <stdbool.h>
#include <raymath.h>

typedef struct {
    void *(*realloc_fn)(void *ud, void *p, size_t sz, size_t align);

    void (*free_fn)(void *ud, void *p);

    void *ud;
} QAllocator;

typedef struct {
    int a, b, c, d;
} Quad;

typedef struct {
    Vector3 *v;
    int vCount, vCap;
    Quad *q;
    int qCount, qCap;
    QAllocator alloc;
} QMesh;

typedef struct {
    Vector3 *v;
    int vCount;
    unsigned *indices;
    int iCount;
} TMesh;

typedef struct {
    int *idx;
    int count, cap;
    QAllocator alloc;
} QRing;

void qm_init(QMesh *m);

void qm_init_with_alloc(QMesh *m, QAllocator alloc);

void qm_set_alloc(QMesh *m, QAllocator alloc);

void qm_free(QMesh *m);

int qm_addv(QMesh *m, Vector3 p);

void qm_addq(QMesh *m, int a, int b, int c, int d);

QRing qr_new(void);

QRing qr_new_with_alloc(QAllocator alloc);

void qr_free(QRing *r);

void qr_push(QRing *r, int i);

QRing ring_ellipse(QMesh *m, float cx, float cy, float rx, float ry, int segs);

QRing ring_grow_out(QMesh *m, const QRing *base, float step, float dz);

void ring_lift_x(QMesh *m, QRing *r, float dx);

void ring_lift_y(QMesh *m, QRing *r, float dy);

void ring_lift_z(QMesh *m, QRing *r, float dz);

QMesh *cap_plane_build(QMesh *b, const QRing *outer, float inset, int steps, int flipWinding,
                       void *(*alloc)(void *, size_t, size_t), void *ud);

bool stitch(QMesh *m, const QRing *a, const QRing *b);

bool stitch_loop(QMesh *m, QRing *rings, int n);

void mesh_merge(QMesh *dst, const QMesh *src);

void mesh_move(QMesh *m, float dx, float dy, float dz);

void mesh_scale(QMesh *m, float sx, float sy, float sz);

void mesh_rotate_x(QMesh *m, float rad);

void mesh_rotate_y(QMesh *m, float rad);

void mesh_rotate_z(QMesh *m, float rad);

void mesh_mirror_x(QMesh *m, float weldEps);

void mesh_mirror_y(QMesh *m, float weldEps);

void mesh_mirror_z(QMesh *m, float weldEps);

void mesh_triangulate_quads(const QMesh *src, TMesh *out, int choose_shortest_diag, int flip_winding,
                            void *(*alloc)(void *, size_t, size_t), void *ud);

void mesh_weld_by_distance(QMesh *m, float eps);

#endif
