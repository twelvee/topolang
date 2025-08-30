#include "eval.h"
#include "ast.h"
#include "intrinsics.h"
#include "util.h"
#include "arena.h"
#include <string.h>
#include "mesh.h"

typedef struct {
    const char *name;
    Value val;
    int isConst;
} Var;
typedef struct {
    Var *vars;
    int vcount, vcap;
    Host host;
    TopoArena *A;
    char err[256];
    int hasRet;
    Value ret;
} Exec;

static void *host_arena_alloc(struct Host *H, size_t sz, size_t align) { return arena_alloc(H->arena, sz, align); }

static Value zero_val() {
    Value z;
    memset(&z, 0, sizeof(z));
    return z;
}

static void setVarEx(Exec *E, const char *name, Value v, int asConst) {
    for (int i = 0; i < E->vcount; i++) {
        if (!strcmp(E->vars[i].name, name)) {
            if (E->vars[i].isConst) {
                strsncpy(E->err, "cannot assign to const", 256);
                return;
            }
            if (asConst) {
                strsncpy(E->err, "redefinition of name", 256);
                return;
            }
            E->vars[i].val = v;
            return;
        }
    }
    if (E->vcount >= E->vcap) {
        int nc = E->vcap ? E->vcap * 2 : 32;
        Var *neu = (Var *) arena_alloc(E->A, sizeof(Var) * (size_t) nc, 8);
        if (E->vars) memcpy(neu, E->vars, sizeof(Var) * (size_t) E->vcount);
        E->vars = neu;
        E->vcap = nc;
    }
    Var vr;
    vr.name = name;
    vr.val = v;
    vr.isConst = asConst ? 1 : 0;
    E->vars[E->vcount++] = vr;
}

static void setVar(Exec *E, const char *name, Value v) { setVarEx(E, name, v, 0); }

static void setConst(Exec *E, const char *name, Value v) { setVarEx(E, name, v, 1); }

static Value getVar(Exec *E, const char *name) {
    for (int i = 0; i < E->vcount; i++) if (!strcmp(E->vars[i].name, name)) return E->vars[i].val;
    return zero_val();
}

static const Builtin *GBI = NULL;
static int GBI_N = 0;

static Value eval_node(Exec *E, Ast *n);

static Value merge_meshes(TopoArena *A, Value a, Value b) {
    if (a.k == VAL_MESH && b.k == VAL_MESH) {
        QMesh *out = (QMesh *) arena_alloc(A, sizeof(QMesh), 8);
        qm_init(out);
        mesh_merge(out, a.mesh);
        mesh_merge(out, b.mesh);
        Value v;
        memset(&v, 0, sizeof(v));
        v.k = VAL_MESH;
        v.mesh = out;
        return v;
    }
    if (a.k == VAL_NUMBER && b.k == VAL_NUMBER) {
        Value v;
        memset(&v, 0, sizeof(v));
        v.k = VAL_NUMBER;
        v.num = a.num + b.num;
        return v;
    }
    return b;
}

static Value eval_call(Exec *E, Ast *n) {
    for (int i = 0; i < GBI_N; i++) {
        if (strcmp(GBI[i].name, n->call.callee) != 0) continue;
        int ac = n->call.args.count;
        Value *argv = (Value *) arena_alloc(E->A, sizeof(Value) * (size_t) ac, 8);
        for (int k = 0; k < ac; k++) argv[k] = eval_node(E, n->call.args.data[k]);
        char emsg[256] = {0};
        Value r = GBI[i].fn(&E->host, argv, ac, emsg);
        if (emsg[0]) strsncpy(E->err, emsg, 256);
        return r;
    }
    return zero_val();
}

static Value eval_node(Exec *E, Ast *n) {
    if (E->hasRet) return zero_val();
    switch (n->kind) {
        case ND_NUM: {
            Value v;
            memset(&v, 0, sizeof(v));
            v.k = VAL_NUMBER;
            v.num = n->num;
            return v;
        }
        case ND_STR: {
            Value v;
            memset(&v, 0, sizeof(v));
            v.k = VAL_STRING;
            v.str.s = n->str;
            return v;
        }
        case ND_IDENT:
            return getVar(E, n->ident.name);
        case ND_CONST: {
            Value r = eval_node(E, n->const_.expr);
            if (E->err[0]) return r;
            setConst(E, n->const_.name, r);
            return zero_val();
        }
        case ND_ASSIGN: {
            Value r = eval_node(E, n->assign.rhs);
            if (E->err[0]) return r;
            setVar(E, n->assign.lhs, r);
            return r;
        }
        case ND_CALL:
            return eval_call(E, n);
        case ND_ARRAY: {
            int nE = n->array.elems.count;
            Value *tmp = (Value *) arena_alloc(E->A, sizeof(Value) * (size_t) nE, 8);
            for (int i = 0; i < nE; i++) tmp[i] = eval_node(E, n->array.elems.data[i]);
            for (int i = 0; i < GBI_N; i++) {
                if (!strcmp(GBI[i].name, "ringlist")) {
                    char er[256] = {0};
                    Value r = GBI[i].fn(&E->host, tmp, nE, er);
                    if (er[0]) strsncpy(E->err, er, 256);
                    return r;
                }
            }
            return zero_val();
        }
        case ND_RETURN: {
            if (n->ret.exprs.count > 0) E->ret = eval_node(E, n->ret.exprs.data[0]); else { E->ret.k = VAL_VOID; }
            E->hasRet = 1;
            return E->ret;
        }
        case ND_ADD: {
            Value L = eval_node(E, n->add.lhs);
            Value R = eval_node(E, n->add.rhs);
            return merge_meshes(E->A, L, R);
        }
        case ND_SUB: {
            Value L = eval_node(E, n->sub.lhs);
            Value R = eval_node(E, n->sub.rhs);
            if (L.k == VAL_NUMBER && R.k == VAL_NUMBER) {
                Value v;
                memset(&v, 0, sizeof(v));
                v.k = VAL_NUMBER;
                v.num = L.num - R.num;
                return v;
            }
            return zero_val();
        }
        case ND_MUL: {
            Value L = eval_node(E, n->mul.lhs);
            Value R = eval_node(E, n->mul.rhs);
            if (L.k == VAL_NUMBER && R.k == VAL_NUMBER) {
                Value v;
                memset(&v, 0, sizeof(v));
                v.k = VAL_NUMBER;
                v.num = L.num * R.num;
                return v;
            }
            return zero_val();
        }
        case ND_DIV: {
            Value L = eval_node(E, n->div.lhs);
            Value R = eval_node(E, n->div.rhs);
            if (L.k == VAL_NUMBER && R.k == VAL_NUMBER) {
                if (R.num == 0) {
                    strsncpy(E->err, "division by zero", 256);
                    return L;
                }
                Value v;
                memset(&v, 0, sizeof(v));
                v.k = VAL_NUMBER;
                v.num = L.num / R.num;
                return v;
            }
            return zero_val();
        }
        case ND_FOR: {
            Value va = eval_node(E, n->for_.from);
            Value vb = eval_node(E, n->for_.to);
            int from = (int) va.num;
            int to = (int) vb.num;
            int step = (from <= to) ? 1 : -1;
            int end = to + (n->for_.inclusive ? 0 : -step);
            for (int i = from;; i += step) {
                Value iv;
                memset(&iv, 0, sizeof(iv));
                iv.k = VAL_NUMBER;
                iv.num = (double) i;
                setVar(E, n->for_.iter, iv);
                (void) eval_node(E, n->for_.body);
                if (E->hasRet) return E->ret;
                if (i == end) break;
            }
            return zero_val();
        }
        case ND_NEG: {
            Value v = eval_node(E, n->un.expr);
            if (v.k == VAL_NUMBER) {
                v.num = -v.num;
                return v;
            }
            return zero_val();
        }
        case ND_BLOCK: {
            Value last;
            memset(&last, 0, sizeof(last));
            for (int i = 0; i < n->block.stmts.count && !E->hasRet; i++) {
                last = eval_node(E, n->block.stmts.data[i]);
                if (E->err[0]) return last;
            }
            return last;
        }
        default:
            return zero_val();
    }
}

bool eval_block_to_value(Ast *block, TopoArena *A, EvalResult *out, char err[256]) {
    Exec E;
    memset(&E, 0, sizeof(E));
    E.A = A;
    E.host.arena = A;
    E.host.build = NULL;
    E.host.alloc = host_arena_alloc;
    E.err[0] = 0;
    E.hasRet = 0;
    int nbi = 0;
    GBI = intrinsics_table(&nbi);
    GBI_N = nbi;
    (void) eval_node(&E, block);
    if (E.err[0]) {
        if (err) strsncpy(err, E.err, 256);
        return false;
    }
    if (!E.hasRet) {
        if (err) strsncpy(err, "create{} did not return", 256);
        return false;
    }
    if (out) {
        out->hasReturn = 1;
        out->ret = E.ret;
    }
    return true;
}
