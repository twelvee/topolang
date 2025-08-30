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

static QRing ring_inset_same_count(QMesh *m, const QRing *base, float dist) {
    QRing out = qr_new_with_alloc(m->alloc);
    Vector3 c = ring_centroid(m, base);
    for (int i = 0; i < base->count; i++) {
        Vector3 p = m->v[base->idx[i]];
        Vector3 d = Vector3Normalize(Vector3Subtract(c, p));
        Vector3 q = (Vector3) {p.x + d.x * dist, p.y + d.y * dist, p.z + d.z * dist};
        qr_push(&out, qm_addv(m, q));
    }
    return out;
}

QMesh *cap_plane_build(QMesh *b, const QRing *outer, void *(*alloc)(void *, size_t, size_t), void *ud) {

    QMesh *cap = (QMesh *) alloc(ud, sizeof(QMesh), 8);
    qm_init_with_alloc(cap, b->alloc);

    const int n = outer->count;
    if (n < 4 || (n % 4) != 0) {
        return cap;
    }

    QRing rim = *outer;

    Vector3 *V = (Vector3 *) alloc(ud, sizeof(Vector3) * (size_t) n, alignof(Vector3));
    for (int i = 0; i < n; i++) V[i] = b->v[rim.idx[i]];

    const int k = n / 4;

    Vector3 *bottom = (Vector3 *) alloc(ud, sizeof(Vector3) * (size_t) (k + 1), alignof(Vector3));
    Vector3 *right = (Vector3 *) alloc(ud, sizeof(Vector3) * (size_t) (k + 1), alignof(Vector3));
    Vector3 *top = (Vector3 *) alloc(ud, sizeof(Vector3) * (size_t) (k + 1), alignof(Vector3));
    Vector3 *left = (Vector3 *) alloc(ud, sizeof(Vector3) * (size_t) (k + 1), alignof(Vector3));

    for (int i = 0; i <= k; i++) bottom[i] = V[0 + i];                 // 0..k
    for (int i = 0; i <= k; i++) right[i] = V[k + i];                 // k..2k
    for (int i = 0; i <= k; i++) top[i] = V[2 * k + (k - i)];         // 2k..3k
    for (int i = 0; i <= k; i++) left[i] = V[(3 * k + (k - i)) % n];   // 3k..4k

    const Vector3 P00 = bottom[0];
    const Vector3 P10 = bottom[k];
    const Vector3 P01 = top[0];
    const Vector3 P11 = top[k];

    const int gw = k + 1, gh = k + 1;
    int *grid = (int *) alloc(ud, sizeof(int) * (size_t) (gw * gh), 4);

    for (int j = 0; j <= k; j++) {
        for (int i = 0; i <= k; i++) {
            const int id = j * gw + i;
            const int onTop = (j == k);
            const int onBottom = (j == 0);
            const int onLeft = (i == 0);
            const int onRight = (i == k);

            if (onTop) { grid[id] = qm_addv(cap, top[i]); }
            else if (onBottom) { grid[id] = qm_addv(cap, bottom[i]); }
            else if (onLeft) { grid[id] = qm_addv(cap, left[j]); }
            else if (onRight) { grid[id] = qm_addv(cap, right[j]); }
            else {
                const float u = (float) i / (float) k;
                const float v = (float) j / (float) k;

                Vector3 C0 = left[j], C1 = right[j];
                Vector3 D0 = bottom[i], D1 = top[i];

                Vector3 term1 = Vector3Lerp(C0, C1, u);
                Vector3 term2 = Vector3Lerp(D0, D1, v);

                Vector3 BL0 = Vector3Lerp(P00, P10, u);
                Vector3 BL1 = Vector3Lerp(P01, P11, u);
                Vector3 BL = Vector3Lerp(BL0, BL1, v);

                Vector3 P = Vector3Subtract(Vector3Add(term1, term2), BL);
                grid[id] = qm_addv(cap, P);
            }
        }
    }

    for (int j = 0; j < k; j++) {
        for (int i = 0; i < k; i++) {
            int A = grid[j * gw + i];
            int B = grid[j * gw + (i + 1)];
            int C = grid[(j + 1) * gw + (i + 1)];
            int D = grid[(j + 1) * gw + i];

            qm_addq(cap, A, B, C, D);
        }
    }

    return cap;
}

void mesh_weld_by_distance(QMesh *m, float eps) {
    if (m->vCount == 0) return;

    typedef struct {
        int keyx, keyy, keyz;
        int head;
    } Cell;
    // todo: use hash map
    int cap = m->vCount * 2 + 64;
    int *next = (int *) malloc(sizeof(int) * (size_t) m->vCount);
    int *head = (int *) malloc(sizeof(int) * (size_t) cap);
    for (int i = 0; i < cap; i++) head[i] = -1;

    float inv = 1.0f / eps;
    int *rep = (int *) malloc(sizeof(int) * (size_t) m->vCount);
    for (int i = 0; i < m->vCount; i++) rep[i] = -1;

    int h = 73856093; // pseudo hash
    for (int i = 0; i < m->vCount; i++) {
        Vector3 p = m->v[i];
        int gx = (int) floorf(p.x * inv), gy = (int) floorf(p.y * inv), gz = (int) floorf(p.z * inv);
        unsigned u = (unsigned) (gx * h ^ gy * 19349663 ^ gz * 83492791);
        int bucket = (int) (u % (unsigned) cap);
        int j = head[bucket];
        int found = -1;
        while (j != -1) {
            Vector3 q = m->v[j];
            float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
            if (dx * dx + dy * dy + dz * dz <= eps * eps) {
                found = j;
                break;
            }
            j = next[j];
        }
        if (found == -1) {
            next[i] = head[bucket];
            head[bucket] = i;
            rep[i] = i;
        } else {
            rep[i] = found;
        }
    }

    for (int qi = 0; qi < m->qCount; qi++) {
        Quad *q = &m->q[qi];
        q->a = rep[q->a];
        q->b = rep[q->b];
        q->c = rep[q->c];
        q->d = rep[q->d];
    }

    int *newIndex = (int *) malloc(sizeof(int) * (size_t) m->vCount);
    int newCount = 0;
    for (int i = 0; i < m->vCount; i++) {
        if (rep[i] == i) newIndex[i] = newCount++;
    }
    Vector3 *nv = (Vector3 *) malloc(sizeof(Vector3) * (size_t) newCount);
    for (int i = 0; i < m->vCount; i++) if (rep[i] == i) nv[newIndex[i]] = m->v[i];
    for (int qi = 0; qi < m->qCount; qi++) {
        Quad *q = &m->q[qi];
        q->a = newIndex[q->a];
        q->b = newIndex[q->b];
        q->c = newIndex[q->c];
        q->d = newIndex[q->d];
    }
    free(m->v);
    m->v = nv;
    m->vCount = newCount;

    free(next);
    free(head);
    free(rep);
    free(newIndex);
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

void mesh_bbox_minmax(const QMesh *m, float *minx, float *miny, float *minz,
                      float *maxx, float *maxy, float *maxz) {
    if (!m || m->vCount <= 0) {
        *minx = *miny = *minz = 0.0f;
        *maxx = *maxy = *maxz = 0.0f;
        return;
    }
    Vector3 v0 = m->v[0];
    float mnx = v0.x, mny = v0.y, mnz = v0.z, mxx = v0.x, mxy = v0.y, mxz = v0.z;
    for (int i = 1; i < m->vCount; i++) {
        Vector3 v = m->v[i];
        if (v.x < mnx) mnx = v.x;
        if (v.y < mny) mny = v.y;
        if (v.z < mnz) mnz = v.z;
        if (v.x > mxx) mxx = v.x;
        if (v.y > mxy) mxy = v.y;
        if (v.z > mxz) mxz = v.z;
    }
    *minx = mnx;
    *miny = mny;
    *minz = mnz;
    *maxx = mxx;
    *maxy = mxy;
    *maxz = mxz;
}