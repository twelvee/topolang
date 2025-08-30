#include "intrinsics.h"
#include <string.h>

static void *arena_realloc(void *ud, void *p, size_t sz, size_t align) {
    Host *H = (Host *) ud;
    if (sz == 0) return p;
    void *np = arena_alloc(H->arena, sz, align);
    if (p && np) memcpy(np, p, sz);
    return np;
}

static void arena_free(void *ud, void *p) {
    (void) ud;
    (void) p;
}

static void *host_alloc_trampoline(void *ud, size_t sz, size_t align) {
    Host *H = (Host *) ud;
    return arena_alloc(H->arena, sz, align);
}

static QAllocator make_arena_alloc(Host *H) {
    QAllocator a = {arena_realloc, arena_free, H};
    return a;
}

static Value VNum(double x) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.k = VAL_NUMBER;
    v.num = x;
    return v;
}

static Value VMes(QMesh *m) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.k = VAL_MESH;
    v.mesh = m;
    return v;
}

static Value VRingV(QRing *r) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.k = VAL_RING;
    v.ring = r;
    return v;
}

static Value VRingListPtrs(QRing **p, int n) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.k = VAL_RINGLIST;
    v.ringlist.ptrs = p;
    v.ringlist.count = n;
    return v;
}

static Value VVoid(void) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.k = VAL_VOID;
    return v;
}

#define ARGNUM(i) (args[i].num)
#define ARGSTR(i) (args[i].str.s)

static QMesh *ensure_builder(Host *H) {
    if (!H->build) {
        H->build = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
        qm_init(H->build);
    }
    qm_set_alloc(H->build, make_arena_alloc(H));
    return H->build;
}

static Value bi_ring(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 5) {
        strcpy(err, "ring(cx,cy,rx,ry,segments)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    QRing *r = (QRing *) arena_alloc(H->arena, sizeof(QRing), 8);
    *r = ring_ellipse(b, (float) ARGNUM(0), (float) ARGNUM(1), (float) ARGNUM(2), (float) ARGNUM(3), (int) ARGNUM(4));
    return VRingV(r);
}

static Value bi_grow_out(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 3 || args[0].k != VAL_RING) {
        strcpy(err, "grow_out(ring, step, dz)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    QRing *outR = (QRing *) arena_alloc(H->arena, sizeof(QRing), 8);
    *outR = ring_grow_out(b, args[0].ring, (float) ARGNUM(1), (float) ARGNUM(2));
    return VRingV(outR);
}

static Value bi_lift_x(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_RING) {
        strcpy(err, "lift_x(ring, dx)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    qm_set_alloc(b, make_arena_alloc(H));
    ring_lift_x(b, args[0].ring, (float) ARGNUM(1));
    return VRingV(args[0].ring);
}

static Value bi_lift_y(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_RING) {
        strcpy(err, "lift_y(ring, dy)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    qm_set_alloc(b, make_arena_alloc(H));
    ring_lift_y(b, args[0].ring, (float) ARGNUM(1));
    return VRingV(args[0].ring);
}

static Value bi_lift_z(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_RING) {
        strcpy(err, "lift_z(ring, dz)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    qm_set_alloc(b, make_arena_alloc(H));
    ring_lift_z(b, args[0].ring, (float) ARGNUM(1));
    return VRingV(args[0].ring);
}

static Value bi_weld(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 1 || args[0].k != VAL_MESH) {
        strcpy(err, "weld(mesh, eps=1e-6)");
        return VVoid();
    }
    double eps = (argc >= 2) ? ARGNUM(1) : 1e-6;
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_weld_by_distance(m, (float) eps);
    return VMes(m);
}

static Value bi_cap_plane(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 1 || args[0].k != VAL_RING) {
        strcpy(err, "cap_plane(ring)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    QMesh *cap = cap_plane_build(b, args[0].ring, host_alloc_trampoline, H);
    return VMes(cap);
}

static Value bi_stitch(Host *H, Value *args, int argc, char err[256]) {
    QMesh *b = ensure_builder(H);

    if (argc == 1 && args[0].k == VAL_RINGLIST) {
        int n = args[0].ringlist.count;
        if (n < 2) {
            QMesh *empty = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
            qm_init(empty);
            return VMes(empty);
        }
        QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
        qm_init(m);

        QRing **src = args[0].ringlist.ptrs;
        QRing *remap = (QRing *) arena_alloc(H->arena, sizeof(QRing) * (size_t) n, 8);

        for (int i = 0; i < n; i++) {
            remap[i].count = src[i]->count;
            remap[i].cap = src[i]->count;
            remap[i].alloc = (QAllocator) {0};
            remap[i].idx = (int *) arena_alloc(H->arena, sizeof(int) * (size_t) src[i]->count, 4);
            for (int k = 0; k < src[i]->count; k++) {
                int old = src[i]->idx[k];
                int neu = qm_addv(m, b->v[old]);
                remap[i].idx[k] = neu;
            }
        }

        for (int i = 0; i < n - 1; i++) stitch(m, &remap[i], &remap[i + 1]);
        return VMes(m);
    }

    if (argc == 2 && args[0].k == VAL_RING && args[1].k == VAL_RING) {
        QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
        qm_init(m);

        const QRing *a = args[0].ring;
        const QRing *bR = args[1].ring;

        QRing A, B;
        A.count = a->count;
        A.cap = a->count;
        A.alloc = (QAllocator) {0};
        B.count = bR->count;
        B.cap = bR->count;
        B.alloc = (QAllocator) {0};

        A.idx = (int *) arena_alloc(H->arena, sizeof(int) * (size_t) a->count, 4);
        B.idx = (int *) arena_alloc(H->arena, sizeof(int) * (size_t) bR->count, 4);

        for (int k = 0; k < a->count; k++) {
            int old = a->idx[k];
            int neu = qm_addv(m, b->v[old]);
            A.idx[k] = neu;
        }
        for (int k = 0; k < bR->count; k++) {
            int old = bR->idx[k];
            int neu = qm_addv(m, b->v[old]);
            B.idx[k] = neu;
        }

        stitch(m, &A, &B);
        return VMes(m);
    }

    strcpy(err, "stitch([rings...]) or stitch(rA, rB)");
    return VVoid();
}

static Value bi_merge(Host *H, Value *args, int argc, char err[256]) {
    for (int i = 0; i < argc; i++) {
        if (args[i].k != VAL_MESH) {
            strcpy(err, "merge(mesh,...)");
            return VVoid();
        }
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    for (int i = 0; i < argc; i++) mesh_merge(m, args[i].mesh);
    return VMes(m);
}

static Value bi_rotate_x(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_MESH) {
        strcpy(err, "rotate_x(mesh, rad)");
        return VVoid();
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_rotate_x(m, (float) ARGNUM(1));
    return VMes(m);
}

static Value bi_rotate_y(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_MESH) {
        strcpy(err, "rotate_y(mesh, rad)");
        return VVoid();
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_rotate_y(m, (float) ARGNUM(1));
    return VMes(m);
}

static Value bi_rotate_z(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_MESH) {
        strcpy(err, "rotate_z(mesh, rad)");
        return VVoid();
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_rotate_z(m, (float) ARGNUM(1));
    return VMes(m);
}

static Value bi_mirror_x(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 1 || args[0].k != VAL_MESH) {
        strcpy(err, "mirror_x(mesh, weld)");
        return VVoid();
    }
    double weld = (argc >= 2) ? ARGNUM(1) : 1e-6;
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_mirror_x(m, (float) weld);
    return VMes(m);
}

static Value bi_mirror_y(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 1 || args[0].k != VAL_MESH) {
        strcpy(err, "mirror_y(mesh, weld)");
        return VVoid();
    }
    double weld = (argc >= 2) ? ARGNUM(1) : 1e-6;
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_mirror_y(m, (float) weld);
    return VMes(m);
}

static Value bi_mirror_z(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 1 || args[0].k != VAL_MESH) {
        strcpy(err, "mirror_z(mesh, weld)");
        return VVoid();
    }
    double weld = (argc >= 2) ? ARGNUM(1) : 1e-6;
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_mirror_z(m, (float) weld);
    return VMes(m);
}

static Value bi_move(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 4 || args[0].k != VAL_MESH) {
        strcpy(err, "move(mesh,dx,dy,dz)");
        return VVoid();
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_move(m, (float) ARGNUM(1), (float) ARGNUM(2), (float) ARGNUM(3));
    return VMes(m);
}

static Value bi_scale(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 4 || args[0].k != VAL_MESH) {
        strcpy(err, "scale(mesh,sx,sy,sz)");
        return VVoid();
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_scale(m, (float) ARGNUM(1), (float) ARGNUM(2), (float) ARGNUM(3));
    return VMes(m);
}

static Value bi_ringlist(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 1) {
        strcpy(err, "ringlist(r0,r1,...)");
        return VVoid();
    }
    for (int i = 0; i < argc; i++) {
        if (args[i].k != VAL_RING) {
            strcpy(err, "ringlist(r0,r1,...) accept only rings");
            return VVoid();
        }
    }
    QRing **arr = (QRing **) arena_alloc(H->arena, sizeof(QRing *) * (size_t) argc, 8);
    for (int i = 0; i < argc; i++) arr[i] = args[i].ring;
    return VRingListPtrs(arr, argc);
}

static Value bi_ringlist_push(Host *H, Value *args, int argc, char err[256]) {
    if (argc != 2 || args[0].k != VAL_RINGLIST || args[1].k != VAL_RING) {
        strcpy(err, "ringlist_push(list, ring)");
        return VVoid();
    }
    int n = args[0].ringlist.count;
    QRing **src = args[0].ringlist.ptrs;
    QRing **arr = (QRing **) arena_alloc(H->arena, sizeof(QRing *) * (size_t) (n + 1), 8);
    if (n > 0) memcpy(arr, src, sizeof(QRing *) * (size_t) n);
    arr[n] = args[1].ring;
    return VRingListPtrs(arr, n + 1);
}

static Value bi_first(Host *H, Value *args, int argc, char err[256]) {
    if (argc != 1 || args[0].k != VAL_RINGLIST || args[0].ringlist.count <= 0) {
        strcpy(err, "first(ringlist)");
        return VVoid();
    }

    return VRingV(args[0].ringlist.ptrs[0]);
}

static Value bi_last(Host *H, Value *args, int argc, char err[256]) {
    if (argc != 1 || args[0].k != VAL_RINGLIST || args[0].ringlist.count <= 0) {
        strcpy(err, "last(ringlist)");
        return VVoid();
    }

    return VRingV(args[0].ringlist.ptrs[args[0].ringlist.count - 1]);
}

static Value bi_vertex(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 3) {
        strcpy(err, "vertex(x,y,z)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    Vector3 p = (Vector3) {(float) ARGNUM(0), (float) ARGNUM(1), (float) ARGNUM(2)};
    int idx = qm_addv(b, p);
    return VNum((double) idx);
}

static Value bi_quad(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 4) {
        strcpy(err, "quad(a,b,c,d)");
        return VVoid();
    }
    QMesh *b = ensure_builder(H);
    int ia = (int) ARGNUM(0), ib = (int) ARGNUM(1), ic = (int) ARGNUM(2), id = (int) ARGNUM(3);
    int vcount = b->vCount;
    if (ia < 0 || ib < 0 || ic < 0 || id < 0 || ia >= vcount || ib >= vcount || ic >= vcount || id >= vcount) {
        strcpy(err, "quad: vertex index out of range");
        return VVoid();
    }
    QMesh *m = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(m);
    int a = qm_addv(m, b->v[ia]);
    int b1 = qm_addv(m, b->v[ib]);
    int c1 = qm_addv(m, b->v[ic]);
    int d = qm_addv(m, b->v[id]);
    qm_addq(m, a, b1, c1, d);
    return VMes(m);
}

static Value bi_mesh(Host *H, Value *args, int argc, char err[256]) {
    if (argc > 0) {
        int have = 0;
        for (int i = 0; i < argc; i++)
            if (args[i].k == VAL_MESH) {
                have = 1;
                break;
            }
        if (have) {
            QMesh *out = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
            qm_init(out);
            for (int i = 0; i < argc; i++) if (args[i].k == VAL_MESH) mesh_merge(out, args[i].mesh);
            return VMes(out);
        }
    }
    QMesh *empty = (QMesh *) arena_alloc(H->arena, sizeof(QMesh), 8);
    qm_init(empty);
    return VMes(empty);
}

static const Builtin BI[] = {
        {"vertex",        bi_vertex},
        {"quad",          bi_quad},
        {"mesh",          bi_mesh},
        {"ring",          bi_ring},
        {"ringlist_push", bi_ringlist_push},
        {"first",         bi_first},
        {"last",          bi_last},
        {"grow_out",      bi_grow_out},
        {"lift_x",        bi_lift_x},
        {"lift_y",        bi_lift_y},
        {"lift_z",        bi_lift_z},
        {"rotate_x",      bi_rotate_x},
        {"rotate_y",      bi_rotate_y},
        {"rotate_z",      bi_rotate_z},
        {"stitch",        bi_stitch},
        {"merge",         bi_merge},
        {"mirror_x",      bi_mirror_x},
        {"mirror_y",      bi_mirror_y},
        {"mirror_z",      bi_mirror_z},
        {"move",          bi_move},
        {"scale",         bi_scale},
        {"ringlist",      bi_ringlist},
        {"cap_plane",     bi_cap_plane},
        {"weld",          bi_weld},

};

const Builtin *intrinsics_table(int *outCount) {
    *outCount = (int) (sizeof(BI) / sizeof(BI[0]));
    return BI;
}
