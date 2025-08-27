#include "topolang.h"
#include "arena.h"
#include "token.h"
#include "ast.h"
#include "util.h"
#include "mesh.h"
#include "eval.h"
#include <string.h>
#include <stdlib.h>

extern AstProgram parse_program(const char *src, TopoArena *A, char err[256], int *line, int *col);

typedef struct {
    const char *name;
    Ast *meshAst;
} MeshEntry;

struct TopoProgram {
    MeshEntry *entries;
    int count, cap;
};

TopoArena *topo_arena_create(size_t bytes) { return arena_create(bytes); }

void topo_arena_reset(TopoArena *A) { arena_reset(A); }

void topo_arena_destroy(TopoArena *A) { arena_destroy(A); }

bool topo_compile(const TopoSource *sources, int nSources, TopoArena *A, TopoProgram **outProg, TopoError *err) {
    TopoProgram *P = (TopoProgram *) arena_alloc(A, sizeof(TopoProgram), 8);
    if (!P) {
        if (err) strsncpy(err->msg, "arena OOM", 256);
        return false;
    }
    memset(P, 0, sizeof(*P));

    for (int i = 0; i < nSources; i++) {
        char emsg[256] = {0};
        int line = 0, col = 0;
        AstProgram pr = parse_program(sources[i].code, A, emsg, &line, &col);
        if (emsg[0]) {
            if (err) {
                err->line = line;
                err->col = col;
                strsncpy(err->msg, emsg, 256);
            }
            return false;
        }
        for (int m = 0; m < pr.count; m++) {
            if (P->count >= P->cap) {
                int nc = P->cap ? P->cap * 2 : 16;
                MeshEntry *neu = (MeshEntry *) arena_alloc(A, sizeof(MeshEntry) * nc, 8);
                if (P->entries) memcpy(neu, P->entries, sizeof(MeshEntry) * P->count);
                P->entries = neu;
                P->cap = nc;
            }
            P->entries[P->count].name = pr.meshes[m]->mesh.name;
            P->entries[P->count].meshAst = pr.meshes[m];
            P->count++;
        }
    }
    *outProg = P;
    return true;
}

bool
topo_execute(const TopoProgram *prog, const char *entryMeshName, TopoArena *A, TopoScene *outScene, TopoError *err) {
    const Ast *mesh = NULL;
    for (int i = 0; i < prog->count; i++)
        if (!strcmp(prog->entries[i].name, entryMeshName))
            mesh = prog->entries[i].meshAst;
    if (!mesh) {
        if (err) strsncpy(err->msg, "mesh not found", 256);
        return false;
    }

    Ast *createBody = NULL;
    for (int i = 0; i < mesh->mesh.items.count; i++) {
        Ast *it = mesh->mesh.items.data[i];
        if (it->kind == ND_CREATE) {
            createBody = it->create.body;
            break;
        }
    }
    if (!createBody) {
        if (err) strsncpy(err->msg, "no create() in mesh", 256);
        return false;
    }

    EvalResult R = {0};
    char emsg[256] = {0};
    if (!eval_block_to_value(createBody, A, &R, emsg)) {
        if (err) strsncpy(err->msg, emsg[0] ? emsg : "eval failed", 256);
        return false;
    }
    if (R.ret.k != VAL_MESH || !R.ret.mesh) {
        if (err) strsncpy(err->msg, "create() did not return mesh", 256);
        return false;
    }

    // QMesh -> TopoMesh
    QMesh *q = R.ret.mesh;
    TopoMesh m = {0};
    m.vCount = q->vCount;
    m.vertices = (float *) malloc(sizeof(float) * 3 * m.vCount);
    for (int i = 0; i < q->vCount; i++) {
        m.vertices[i * 3 + 0] = q->v[i].x;
        m.vertices[i * 3 + 1] = q->v[i].y;
        m.vertices[i * 3 + 2] = q->v[i].z;
    }
    m.qCount = q->qCount;
    m.quads = (int *) malloc(sizeof(int) * 4 * m.qCount);
    for (int i = 0; i < q->qCount; i++) {
        m.quads[i * 4 + 0] = q->q[i].a;
        m.quads[i * 4 + 1] = q->q[i].b;
        m.quads[i * 4 + 2] = q->q[i].c;
        m.quads[i * 4 + 3] = q->q[i].d;
    }

    outScene->count = 1;
    outScene->meshes = (TopoMesh *) malloc(sizeof(TopoMesh));
    outScene->meshes[0] = m;
    return true;
}

void topo_free_mesh(TopoMesh *m) {
    if (!m) return;
    free(m->vertices);
    free(m->quads);
    m->vertices = NULL;
    m->quads = NULL;
    m->vCount = m->qCount = 0;
}

void topo_free_scene(TopoScene *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) topo_free_mesh(&s->meshes[i]);
    free(s->meshes);
    s->meshes = NULL;
    s->count = 0;
}
