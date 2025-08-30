#include "topolang.h"
#include "arena.h"
#include "token.h"
#include "ast.h"
#include "util.h"
#include "mesh.h"
#include "eval.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern AstProgram parse_program(const char *src, TopoArena *A, char err[256], int *line, int *col);

typedef struct {
    const char *name;
    Ast *meshAst;
} MeshEntry;

struct TopoProgram {
    MeshEntry *entries;
    int count, cap;
    Ast **globals;
    int gcount, gcap;
};

typedef struct {
    const char *path;
    AstProgram pr;
    unsigned state;
} Module;

typedef struct {
    Module *data;
    int count, cap;
} ModuleVec;

static void modulevec_push(TopoArena *A, ModuleVec *v, Module m) {
    if (v->count >= v->cap) {
        int nc = v->cap ? v->cap * 2 : 8;
        Module *neu = (Module *) arena_alloc(A, sizeof(Module) * nc, 8);
        if (v->data) memcpy(neu, v->data, sizeof(Module) * v->count);
        v->data = neu;
        v->cap = nc;
    }
    v->data[v->count++] = m;
}

static int modulevec_find(ModuleVec *v, const char *path) {
    for (int i = 0; i < v->count; i++) {
        if (v->data[i].path && !strcmp(v->data[i].path, path)) return i;
    }
    return -1;
}

static void push_mesh_entry(TopoProgram *P, TopoArena *A, const char *name, Ast *meshAst) {
    if (P->count >= P->cap) {
        int nc = P->cap ? P->cap * 2 : 16;
        MeshEntry *neu = (MeshEntry *) arena_alloc(A, sizeof(MeshEntry) * nc, 8);
        if (P->entries) memcpy(neu, P->entries, sizeof(MeshEntry) * P->count);
        P->entries = neu;
        P->cap = nc;
    }
    P->entries[P->count].name = name;
    P->entries[P->count].meshAst = meshAst;
    P->count++;
}

static void push_global(TopoProgram *P, TopoArena *A, Ast *g) {
    if (P->gcount >= P->gcap) {
        int nc = P->gcap ? P->gcap * 2 : 16;
        Ast **neu = (Ast **) arena_alloc(A, sizeof(Ast *) * nc, 8);
        if (P->globals) memcpy(neu, P->globals, sizeof(Ast *) * P->gcount);
        P->globals = neu;
        P->gcap = nc;
    }
    P->globals[P->gcount++] = g;
}

static void astlist_push(TopoArena *A, AstList *L, Ast *x) {
    if (L->count >= L->cap) {
        int nc = L->cap ? L->cap * 2 : 8;
        Ast **neu = (Ast **) arena_alloc(A, sizeof(Ast *) * nc, 8);
        if (L->data) memcpy(neu, L->data, sizeof(Ast *) * L->count);
        L->data = neu;
        L->cap = nc;
    }
    L->data[L->count++] = x;
}

TopoArena *topo_arena_create(size_t bytes) { return arena_create(bytes); }

void topo_arena_reset(TopoArena *A) { arena_reset(A); }

void topo_arena_destroy(TopoArena *A) { arena_destroy(A); }

static char *arena_strdup(TopoArena *A, const char *s) {
    size_t n = strlen(s);
    char *d = (char *) arena_alloc(A, n + 1, 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

static int path_is_abs(const char *p) {
    if (!p || !p[0]) return 0;
#ifdef _WIN32
    if ((p[0] && p[1] == ':') || p[0] == '\\' || p[0] == '/') return 1;
    return 0;
#else
    return p[0] == '/';
#endif
}

static char *resolve_path(TopoArena *A, const char *baseFile, const char *rel) {
    if (path_is_abs(rel) || !baseFile || !baseFile[0]) return arena_strdup(A, rel);
    const char *slash = strrchr(baseFile, '/');
#ifdef _WIN32
    const char *bslash = strrchr(baseFile, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return arena_strdup(A, rel);
    size_t dirlen = (size_t) (slash - baseFile + 1);
    size_t rlen = strlen(rel);
    char *out = (char *) arena_alloc(A, dirlen + rlen + 1, 1);
    if (!out) return NULL;
    memcpy(out, baseFile, dirlen);
    memcpy(out + dirlen, rel, rlen);
    out[dirlen + rlen] = 0;
    return out;
}

static char *slurp_file_to_arena(TopoArena *A, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *) arena_alloc(A, (size_t) sz + 1, 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    buf[rd] = 0;
    fclose(f);
    return buf;
}

static bool load_module_recursive(const TopoSource *sources, int nSources,
                                  TopoArena *A, ModuleVec *mods,
                                  const char *path, const char *importerPath,
                                  TopoError *err) {
    char *resolved = arena_strdup(A, path ? path : "");
    if (!path_is_abs(resolved)) resolved = resolve_path(A, importerPath, resolved);

    int idx = modulevec_find(mods, resolved);
    if (idx >= 0) {
        if (mods->data[idx].state == 1) {
            if (err) strsncpy(err->msg, "import cycle detected", 256);
            return false;
        }
        return true;
    }

    const char *code = NULL;
    for (int i = 0; i < nSources; i++) {
        if (sources[i].path && !strcmp(sources[i].path, resolved)) {
            code = sources[i].code;
            break;
        }
    }
    if (!code) code = slurp_file_to_arena(A, resolved);
    if (!code) {
        if (err) {
            err->line = 0;
            err->col = 0;
            snprintf(err->msg, 256, "%s: import not found", resolved);
        }
        return false;
    }

    Module m = (Module) {0};
    m.path = resolved;
    m.state = 1;
    modulevec_push(A, mods, m);

    char emsg[256] = {0};
    int line = 0, col = 0;
    AstProgram pr = parse_program(code, A, emsg, &line, &col);
    if (emsg[0]) {
        if (err) {
            err->line = line;
            err->col = col;
            snprintf(err->msg, 256, "%s:%d:%d %s", resolved, line, col, emsg);
        }
        return false;
    }

    int pos = modulevec_find(mods, resolved);
    mods->data[pos].pr = pr;

    for (int g = 0; g < pr.gcount; g++) {
        Ast *it = pr.globals[g];
        if (it && it->kind == ND_IMPORT) {
            if (!load_module_recursive(sources, nSources, A, mods, it->import_.path, resolved, err))
                return false;
        }
    }

    mods->data[pos].state = 2;
    return true;
}

bool topo_compile(const TopoSource *sources, int nSources,
                  TopoArena *A, TopoProgram **outProg, TopoError *err) {
    TopoProgram *P = (TopoProgram *) arena_alloc(A, sizeof(TopoProgram), 8);
    if (!P) {
        if (err) strsncpy(err->msg, "arena OOM", 256);
        return false;
    }
    memset(P, 0, sizeof(*P));

    ModuleVec mods = (ModuleVec) {0};

    for (int i = 0; i < nSources; i++) {
        if (!load_module_recursive(sources, nSources, A, &mods, sources[i].path, NULL, err))
            return false;
    }

    for (int i = 0; i < mods.count; i++) {
        AstProgram pr = mods.data[i].pr;
        for (int g = 0; g < pr.gcount; g++) push_global(P, A, pr.globals[g]);
        for (int m = 0; m < pr.count; m++) push_mesh_entry(P, A, pr.meshes[m]->mesh.name, pr.meshes[m]);
    }

    *outProg = P;
    return true;
}
static char *a_strdup(TopoArena *A, const char *s) {
    size_t n = strlen(s);
    char *d = (char *) arena_alloc(A, n + 1, 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

static Ast *wrap_part_as_func(TopoArena *A, const char *fname, const NdPart *part) {
    Ast *fn = (Ast *) arena_alloc(A, sizeof(Ast), 8);
    memset(fn, 0, sizeof(*fn));
    fn->kind = ND_FUNC;
    fn->func.name = a_strdup(A, fname);

    int pc = part->pcount;
    if (pc > 0) {
        FParam *pars = (FParam *) arena_alloc(A, sizeof(FParam) * (size_t) pc, 8);
        for (int i = 0; i < pc; i++) {
            const char *ty = part->params[i].type ? part->params[i].type : "number";
            pars[i].type = a_strdup(A, ty);
            pars[i].name = part->params[i].name;
        }
        fn->func.params = pars;
        fn->func.pcount = pc;
    } else {
        fn->func.params = NULL;
        fn->func.pcount = 0;
    }

    fn->func.ret_type = a_strdup(A, "mesh");

    Ast *blk = (Ast *) arena_alloc(A, sizeof(Ast), 8);
    memset(blk, 0, sizeof(*blk));
    blk->kind = ND_BLOCK;
    blk->line = part->body ? part->body->line : 0;
    blk->col  = part->body ? part->body->col  : 0;

    for (int i = 0; i < part->pcount; i++) {
        if (part->params[i].value) {
            Ast *as = (Ast *) arena_alloc(A, sizeof(Ast), 8);
            memset(as, 0, sizeof(*as));
            as->kind = ND_ASSIGN;
            as->assign.lhs = part->params[i].name;
            as->assign.rhs = part->params[i].value;
            astlist_push(A, &blk->block.stmts, as);
        }
    }

    astlist_push(A, &blk->block.stmts, part->body);

    Ast *ret = (Ast *) arena_alloc(A, sizeof(Ast), 8);
    memset(ret, 0, sizeof(*ret));
    ret->kind = ND_RETURN;
    ret->ret.exprs.count = 1;
    ret->ret.exprs.cap   = 1;
    ret->ret.exprs.data  = (Ast **) arena_alloc(A, sizeof(Ast *), 8);
    ret->ret.exprs.data[0] = part->body;
    astlist_push(A, &blk->block.stmts, ret);

    fn->func.body = blk;
    fn->line = blk->line;
    fn->col  = blk->col;
    return fn;
}

static void inject_parts_for_mesh(TopoArena *A, const Ast *meshAst, AstList *dst, int qualify) {
    const char *prefix = meshAst->mesh.name;
    size_t plen = prefix ? strlen(prefix) : 0;
    for (int i = 0; i < meshAst->mesh.items.count; i++) {
        Ast *it = meshAst->mesh.items.data[i];
        if (it->kind != ND_PART) continue;

        if (!qualify) {
            Ast *fn = wrap_part_as_func(A, it->part.name, &it->part);
            astlist_push(A, dst, fn);
        } else {
            const char *pn = it->part.name;
            size_t nlen = strlen(pn);
            char *q = (char *) arena_alloc(A, plen + 1 + nlen + 1, 1);
            memcpy(q, prefix, plen);
            q[plen] = '.';
            memcpy(q + plen + 1, pn, nlen);
            q[plen + 1 + nlen] = 0;
            Ast *fn = wrap_part_as_func(A, q, &it->part);
            astlist_push(A, dst, fn);
        }
    }
}

bool topo_execute(const TopoProgram *prog, const char *entryMeshName,
                  TopoArena *A, TopoScene *outScene, TopoError *err) {
    const Ast *mesh = NULL;
    for (int i = 0; i < prog->count; i++) {
        if (!strcmp(prog->entries[i].name, entryMeshName)) { mesh = prog->entries[i].meshAst; break; }
    }
    if (!mesh) {
        if (err) strsncpy(err->msg, "mesh not found", 256);
        return false;
    }

    Ast *createBody = NULL;
    for (int i = 0; i < mesh->mesh.items.count; i++) {
        Ast *it = mesh->mesh.items.data[i];
        if (it->kind == ND_CREATE) { createBody = it->create.body; break; }
    }
    if (!createBody) {
        if (err) strsncpy(err->msg, "no create() in mesh", 256);
        return false;
    }

    Ast *wrapper = (Ast *) arena_alloc(A, sizeof(Ast), 8);
    memset(wrapper, 0, sizeof(*wrapper));
    wrapper->kind = ND_BLOCK;
    wrapper->line = createBody->line;
    wrapper->col  = createBody->col;

    inject_parts_for_mesh(A, mesh, &wrapper->block.stmts, 0);
    for (int i = 0; i < prog->count; i++)
        inject_parts_for_mesh(A, prog->entries[i].meshAst, &wrapper->block.stmts, 1);

    for (int i = 0; i < prog->gcount; i++)
        astlist_push(A, &wrapper->block.stmts, prog->globals[i]);

    for (int i = 0; i < mesh->mesh.items.count; i++) {
        Ast *it = mesh->mesh.items.data[i];
        if (it->kind == ND_CONST || it->kind == ND_FUNC)
            astlist_push(A, &wrapper->block.stmts, it);
    }

    astlist_push(A, &wrapper->block.stmts, createBody);

    EvalResult R = (EvalResult){0};
    char emsg[256] = {0};
    if (!eval_block_to_value(wrapper, A, &R, emsg)) {
        if (err) strsncpy(err->msg, emsg[0] ? emsg : "eval failed", 256);
        return false;
    }
    if (R.ret.k != VAL_MESH || !R.ret.mesh) {
        if (err) strsncpy(err->msg, "create() did not return mesh", 256);
        return false;
    }

    QMesh *q = R.ret.mesh;
    TopoMesh m = (TopoMesh){0};
    m.vCount = q->vCount;
    m.vertices = (float *) malloc(sizeof(float) * 3 * (size_t) m.vCount);
    for (int i = 0; i < q->vCount; i++) {
        m.vertices[i * 3 + 0] = q->v[i].x;
        m.vertices[i * 3 + 1] = q->v[i].y;
        m.vertices[i * 3 + 2] = q->v[i].z;
    }
    m.qCount = q->qCount;
    m.quads = (int *) malloc(sizeof(int) * 4 * (size_t) m.qCount);
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
