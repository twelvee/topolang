#include "intrinsics.h"
#include <string.h>

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

static Value VRingList(QRing *r, int n) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.k = VAL_RINGLIST;
    v.ringlist.data = r;
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
        H->build = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
        qm_init(H->build);
    }
    return H->build;
}

static Value bi_ring(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 5) {
        strcpy(err, "ring(cx,cy,rx,ry,segments)");
        return VVoid();
    }
    QMesh *tmp = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(tmp);
    QRing *r = (QRing *) H->alloc(H, sizeof(QRing), 8);
    *r = ring_ellipse(tmp, (float) ARGNUM(0), (float) ARGNUM(1), (float) ARGNUM(2), (float) ARGNUM(3), (int) ARGNUM(4));
    return VRingV(r);
}

static Value bi_grow_out(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 3 || args[0].k != VAL_RING) {
        strcpy(err, "grow_out(ring, step, dz)");
        return VVoid();
    }
    QMesh *tmp = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(tmp);
    QRing *outR = (QRing *) H->alloc(H, sizeof(QRing), 8);
    *outR = ring_grow_out(tmp, args[0].ring, (float) ARGNUM(1), (float) ARGNUM(2));
    return VRingV(outR);
}

static Value bi_lift_x(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_RING) {
        strcpy(err, "lift_x(ring, dx)");
        return VVoid();
    }
    QMesh *tmp = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(tmp);
    QRing *r = args[0].ring;
    ring_lift_x(tmp, r, (float) ARGNUM(1));
    return VRingV(r);
}

static Value bi_lift_y(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_RING) {
        strcpy(err, "lift_y(ring, dy)");
        return VVoid();
    }
    QMesh *tmp = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(tmp);
    QRing *r = args[0].ring;
    ring_lift_y(tmp, r, (float) ARGNUM(1));
    return VRingV(r);
}

static Value bi_lift_z(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_RING) {
        strcpy(err, "lift_z(ring, dz)");
        return VVoid();
    }
    QMesh *tmp = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(tmp);
    QRing *r = args[0].ring;
    ring_lift_z(tmp, r, (float) ARGNUM(1));
    return VRingV(r);
}

static Value bi_stitch(Host *H, Value *args, int argc, char err[256]) {
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(m);
    if (argc == 1 && args[0].k == VAL_RINGLIST) {
        for (int i = 0; i < args[0].ringlist.count - 1; i++)
            stitch(m, &args[0].ringlist.data[i], &args[0].ringlist.data[i + 1]);
        return VMes(m);
    }
    if (argc == 2 && args[0].k == VAL_RING && args[1].k == VAL_RING) {
        stitch(m, args[0].ring, args[1].ring);
        return VMes(m);
    }
    strcpy(err, "stitch([rings...]) или stitch(rA,rB)");
    return VVoid();
}

static Value bi_merge(Host *H, Value *args, int argc, char err[256]) {
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(m);
    for (int i = 0; i < argc; i++) {
        if (args[i].k != VAL_MESH) {
            strcpy(err, "merge(mesh,...)");
            return VVoid();
        }
        mesh_merge(m, args[i].mesh);
    }
    return VMes(m);
}

static Value bi_rotate_x(Host *H, Value *args, int argc, char err[256]) {
    if (argc < 2 || args[0].k != VAL_MESH) {
        strcpy(err, "rotate_x(mesh, rad)");
        return VVoid();
    }
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
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
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(m);
    mesh_merge(m, args[0].mesh);
    mesh_scale(m, (float) ARGNUM(1), (float) ARGNUM(2), (float) ARGNUM(3));
    return VMes(m);
}

static Value bi_ringlist(Host *H, Value *args, int argc, char err[256]) {
    QRing *arr = (QRing *) H->alloc(H, sizeof(QRing) * (size_t) argc, 8);
    for (int i = 0; i < argc; i++) {
        if (args[i].k != VAL_RING) {
            strcpy(err, "ringlist(r0,r1,...) accept only rings");
            return VVoid();
        }
        arr[i] = *args[i].ring;
    }
    return VRingList(arr, argc);
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
    if (ia < 0 || ib < 0 || ic < 0 || id < 0 || ia >= qm_addv(b, (Vector3) {0, 0, 0}) - 1) {}
    QMesh *m = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(m);
    int a = qm_addv(m, ((Vector3 *) (&b->v[0]))[ia]);
    int b1 = qm_addv(m, ((Vector3 *) (&b->v[0]))[ib]);
    int c1 = qm_addv(m, ((Vector3 *) (&b->v[0]))[ic]);
    int d = qm_addv(m, ((Vector3 *) (&b->v[0]))[id]);
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
            QMesh *out = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
            qm_init(out);
            for (int i = 0; i < argc; i++) if (args[i].k == VAL_MESH) mesh_merge(out, args[i].mesh);
            return VMes(out);
        }
    }
    QMesh *empty = (QMesh *) H->alloc(H, sizeof(QMesh), 8);
    qm_init(empty);
    return VMes(empty);
}

static const Builtin BI[] = {
        {"vertex",   bi_vertex},
        {"quad",     bi_quad},
        {"mesh",     bi_mesh},
        {"ring",     bi_ring},
        {"grow_out", bi_grow_out},
        {"lift_x",   bi_lift_x},
        {"lift_y",   bi_lift_y},
        {"lift_z",   bi_lift_z},
        {"rotate_x", bi_rotate_x},
        {"rotate_y", bi_rotate_y},
        {"rotate_z", bi_rotate_z},
        {"stitch",   bi_stitch},
        {"merge",    bi_merge},
        {"mirror_x", bi_mirror_x},
        {"mirror_y", bi_mirror_y},
        {"mirror_z", bi_mirror_z},
        {"move",     bi_move},
        {"scale",    bi_scale},
        {"ringlist", bi_ringlist},
};

const Builtin *intrinsics_table(int *outCount) {
    *outCount = (int) (sizeof(BI) / sizeof(BI[0]));
    return BI;
}
