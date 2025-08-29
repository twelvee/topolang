#include "mesh.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdalign.h>

static void *sys_realloc(void *ud, void *p, size_t sz, size_t align) {
    (void) ud;
    (void) align;
    void *q = realloc(p, sz);
    if (!q && sz) abort();
    return q;
}

static void sys_free(void *ud, void *p) {
    (void) ud;
    free(p);
}

static QAllocator QALLOC_SYS = {sys_realloc, sys_free, NULL};

static void *qa_realloc(QAllocator *a, void *p, size_t sz, size_t align) {
    return a->realloc_fn ? a->realloc_fn(a->ud, p, sz, align) : sys_realloc(NULL, p, sz, align);
}

static void qa_free(QAllocator *a, void *p) {
    if (a->free_fn) a->free_fn(a->ud, p);
    else sys_free(NULL, p);
}

void qm_init(QMesh *m) {
    memset(m, 0, sizeof(*m));
    m->alloc = QALLOC_SYS;
}

void qm_init_with_alloc(QMesh *m, QAllocator alloc) {
    memset(m, 0, sizeof(*m));
    m->alloc = alloc.realloc_fn ? alloc : QALLOC_SYS;
}

void qm_set_alloc(QMesh *m, QAllocator alloc) {
    m->alloc = alloc.realloc_fn ? alloc : QALLOC_SYS;
}

void qm_free(QMesh *m) {
    qa_free(&m->alloc, m->v);
    qa_free(&m->alloc, m->q);
    m->v = m->q = NULL;
    m->vCount = m->vCap = m->qCount = m->qCap = 0;
}

int qm_addv(QMesh *m, Vector3 p) {
    if (m->vCount >= m->vCap) {
        int newCap = m->vCap ? m->vCap * 2 : 256;
        m->v = (Vector3 *) qa_realloc(&m->alloc, m->v, sizeof(Vector3) * (size_t) newCap, alignof(Vector3));
        m->vCap = newCap;
    }
    m->v[m->vCount] = p;
    return m->vCount++;
}

void qm_addq(QMesh *m, int a, int b, int c, int d) {
    if (m->qCount >= m->qCap) {
        int newCap = m->qCap ? m->qCap * 2 : 256;
        m->q = (Quad *) qa_realloc(&m->alloc, m->q, sizeof(Quad) * (size_t) newCap, alignof(Quad));
        m->qCap = newCap;
    }
    m->q[m->qCount++] = (Quad) {a, b, c, d};
}

QRing qr_new(void) {
    QRing r;
    memset(&r, 0, sizeof(r));
    r.alloc = QALLOC_SYS;
    return r;
}

QRing qr_new_with_alloc(QAllocator alloc) {
    QRing r;
    memset(&r, 0, sizeof(r));
    r.alloc = alloc.realloc_fn ? alloc : QALLOC_SYS;
    return r;
}

void qr_free(QRing *r) {
    qa_free(&r->alloc, r->idx);
    r->idx = NULL;
    r->count = r->cap = 0;
}

void qr_push(QRing *r, int i) {
    if (r->count >= r->cap) {
        int newCap = r->cap ? r->cap * 2 : 64;
        r->idx = (int *) qa_realloc(&r->alloc, r->idx, sizeof(int) * (size_t) newCap, alignof(
        int));
        r->cap = newCap;
    }
    r->idx[r->count++] = i;
}

QRing ring_ellipse(QMesh *m, float cx, float cy, float rx, float ry, int segs) {
    QRing r = qr_new_with_alloc(m->alloc);
    for (int k = 0; k < segs; k++) {
        float t = (float) k / (float) segs * 2.0f * PI;
        Vector3 p = (Vector3) {cx + rx * cosf(t), cy + ry * sinf(t), 0.0f};
        qr_push(&r, qm_addv(m, p));
    }
    return r;
}

static Vector3 ring_centroid(const QMesh *m, const QRing *r) {
    Vector3 c = (Vector3) {0, 0, 0};
    for (int i = 0; i < r->count; i++) c = Vector3Add(c, m->v[r->idx[i]]);
    if (r->count > 0) c = Vector3Scale(c, 1.0f / (float) r->count);
    return c;
}

QRing ring_grow_out(QMesh *m, const QRing *base, float step, float dz) {
    QRing out = qr_new_with_alloc(m->alloc);
    Vector3 c = ring_centroid(m, base);
    for (int i = 0; i < base->count; i++) {
        Vector3 p = m->v[base->idx[i]];
        Vector3 n = Vector3Normalize(Vector3Subtract(p, c));
        Vector3 q = (Vector3) {p.x + n.x * step, p.y + n.y * step, p.z + dz};
        qr_push(&out, qm_addv(m, q));
    }
    return out;
}

void ring_lift_x(QMesh *m, QRing *r, float dx) {
    for (int i = 0; i < r->count; i++) {
        int id = r->idx[i];
        m->v[id].x += dx;
    }
}

void ring_lift_y(QMesh *m, QRing *r, float dy) {
    for (int i = 0; i < r->count; i++) {
        int id = r->idx[i];
        m->v[id].y += dy;
    }
}

void ring_lift_z(QMesh *m, QRing *r, float dz) {
    for (int i = 0; i < r->count; i++) {
        int id = r->idx[i];
        m->v[id].z += dz;
    }
}

bool stitch(QMesh *m, const QRing *a, const QRing *b) {
    if (a->count != b->count) return false;
    int n = a->count;
    for (int i = 0; i < n; i++) {
        int A = a->idx[i];
        int B = a->idx[(i + 1) % n];
        int C = b->idx[(i + 1) % n];
        int D = b->idx[i];
        qm_addq(m, A, B, C, D);
    }
    return true;
}

bool stitch_loop(QMesh *m, QRing *rings, int n) {
    for (int i = 0; i < n - 1; i++) if (!stitch(m, &rings[i], &rings[i + 1])) return false;
    return true;
}

void mesh_merge(QMesh *dst, const QMesh *src) {
    int off = dst->vCount;
    for (int i = 0; i < src->vCount; i++) qm_addv(dst, src->v[i]);
    for (int i = 0; i < src->qCount; i++) {
        Quad q = src->q[i];
        qm_addq(dst, q.a + off, q.b + off, q.c + off, q.d + off);
    }
}

void mesh_move(QMesh *m, float dx, float dy, float dz) {
    for (int i = 0; i < m->vCount; i++) {
        m->v[i].x += dx;
        m->v[i].y += dy;
        m->v[i].z += dz;
    }
}

void mesh_scale(QMesh *m, float sx, float sy, float sz) {
    for (int i = 0; i < m->vCount; i++) {
        m->v[i].x *= sx;
        m->v[i].y *= sy;
        m->v[i].z *= sz;
    }
}

void mesh_rotate_x(QMesh *m, float rad) {
    float c = cosf(rad), s = sinf(rad);
    for (int i = 0; i < m->vCount; i++) {
        float y = m->v[i].y, z = m->v[i].z;
        m->v[i].y = y * c - z * s;
        m->v[i].z = y * s + z * c;
    }
}

void mesh_rotate_y(QMesh *m, float rad) {
    float c = cosf(rad), s = sinf(rad);
    for (int i = 0; i < m->vCount; i++) {
        float x = m->v[i].x, z = m->v[i].z;
        m->v[i].x = x * c + z * s;
        m->v[i].z = -x * s + z * c;
    }
}

void mesh_rotate_z(QMesh *m, float rad) {
    float c = cosf(rad), s = sinf(rad);
    for (int i = 0; i < m->vCount; i++) {
        float x = m->v[i].x, y = m->v[i].y;
        m->v[i].x = x * c - y * s;
        m->v[i].y = x * s + y * c;
    }
}

void mesh_mirror_x(QMesh *m, float weldEps) {
    int v0 = m->vCount;
    for (int i = 0; i < v0; i++) {
        Vector3 p = m->v[i];
        p.x = -p.x;
        qm_addv(m, p);
    }
    int q0 = m->qCount;
    for (int i = 0; i < q0; i++) {
        Quad q = m->q[i];
        qm_addq(m, q.d + v0, q.c + v0, q.b + v0, q.a + v0);
    }
    for (int i = 0; i < m->vCount; i++) if (fabsf(m->v[i].x) < weldEps) m->v[i].x = 0.f;
}

void mesh_mirror_y(QMesh *m, float weldEps) {
    int v0 = m->vCount;
    for (int i = 0; i < v0; i++) {
        Vector3 p = m->v[i];
        p.y = -p.y;
        qm_addv(m, p);
    }
    int q0 = m->qCount;
    for (int i = 0; i < q0; i++) {
        Quad q = m->q[i];
        qm_addq(m, q.d + v0, q.c + v0, q.b + v0, q.a + v0);
    }
    for (int i = 0; i < m->vCount; i++) if (fabsf(m->v[i].y) < weldEps) m->v[i].y = 0.f;
}

void mesh_mirror_z(QMesh *m, float weldEps) {
    int v0 = m->vCount;
    for (int i = 0; i < v0; i++) {
        Vector3 p = m->v[i];
        p.z = -p.z;
        qm_addv(m, p);
    }
    int q0 = m->qCount;
    for (int i = 0; i < q0; i++) {
        Quad q = m->q[i];
        qm_addq(m, q.d + v0, q.c + v0, q.b + v0, q.a + v0);
    }
    for (int i = 0; i < m->vCount; i++) if (fabsf(m->v[i].z) < weldEps) m->v[i].z = 0.f;
}
