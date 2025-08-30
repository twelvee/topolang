#include "token.h"
#include "ast.h"
#include "arena.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    Lexer L;
    Token t;
    TopoArena *A;
    const char *file;
    char err[256];
    int errLine, errCol;
    int hasErr;
} Parser;

static const char *g_parse_filename = NULL;

void parser_set_filename(const char *fn) { g_parse_filename = fn; }

static void *Aalloc(TopoArena *A, size_t sz) { return arena_alloc(A, sz, 8); }

static void next_tok(Parser *P) { P->t = lex_next(&P->L); }

static int accept(Parser *P, TokenKind k) {
    if (P->t.kind == k) {
        next_tok(P);
        return 1;
    }
    return 0;
}

static void expect(Parser *P, TokenKind k, const char *what) {
    if (!accept(P, k)) {
        snprintf(P->err, sizeof(P->err), "expected %s", what);
        P->hasErr = 1;
        P->errLine = P->t.line;
        P->errCol = P->t.col;
    }
}

static void skip_nl(Parser *P) { while (P->t.kind == TK_NEWLINE) next_tok(P); }

static char *dupLex(Parser *P, const Token *t) {
    char *s = (char *) Aalloc(P->A, (size_t) t->len + 1);
    memcpy(s, t->lexeme, (size_t) t->len);
    s[t->len] = 0;
    return s;
}

static Ast *newNode(Parser *P, NodeKind k) {
    Ast *n = (Ast *) Aalloc(P->A, sizeof(Ast));
    memset(n, 0, sizeof(*n));
    n->kind = k;
    n->line = P->t.line;
    n->col = P->t.col;
    n->file = P->file;
    return n;
}

static void list_push_ast(TopoArena *A, AstList *L, Ast *x) {
    if (L->count >= L->cap) {
        int nc = L->cap ? L->cap * 2 : 8;
        Ast **neu = (Ast **) Aalloc(A, sizeof(Ast *) * nc);
        if (L->data) memcpy(neu, L->data, sizeof(Ast *) * L->count);
        L->data = neu;
        L->cap = nc;
    }
    L->data[L->count++] = x;
}

static void skip_annotation_to_lbrace(Parser *P) {
    if (!accept(P, TK_COLON)) return;
    skip_nl(P);
    if (P->t.kind == TK_IDENT || P->t.kind == TK_MESH) next_tok(P);
    skip_nl(P);
}

static Ast *parse_expr(Parser *P);

static Ast *parse_statement(Parser *P);

static Ast *parse_for(Parser *P);

static Ast *parse_if(Parser *P);

static Ast *parse_block(Parser *P);

static Ast *parse_func(Parser *P);

static int is_func_decl(Parser *P) {
    Parser Q = *P;
    if (Q.t.kind != TK_IDENT) return 0;
    next_tok(&Q);
    skip_nl(&Q);
    if (!accept(&Q, TK_LPAREN)) return 0;
    skip_nl(&Q);
    if (!accept(&Q, TK_RPAREN)) {
        for (;;) {
            if (Q.t.kind != TK_IDENT) return 0;
            next_tok(&Q);
            skip_nl(&Q);
            if (Q.t.kind != TK_IDENT) return 0;
            next_tok(&Q);
            skip_nl(&Q);
            if (accept(&Q, TK_COMMA)) {
                skip_nl(&Q);
                continue;
            }
            break;
        }
        if (!accept(&Q, TK_RPAREN)) return 0;
    }
    skip_nl(&Q);
    if (!accept(&Q, TK_COLON)) return 0;
    skip_nl(&Q);
    if (!is_type_token(Q.t.kind)) return 0;
    next_tok(&Q);
    skip_nl(&Q);
    if (!accept(&Q, TK_LBRACE)) return 0;
    return 1;
}

static char *parse_qualified_name(Parser *P, Token firstIdent) {
    int total = firstIdent.len;
    int parts = 1;
    const char *segments[32];
    int seglen[32];
    segments[0] = firstIdent.lexeme;
    seglen[0] = firstIdent.len;

    Parser Q = *P;
    while (Q.t.kind == TK_DOT) {
        next_tok(&Q);
        if (Q.t.kind != TK_IDENT) break;
        if (parts < 32) {
            segments[parts] = Q.t.lexeme;
            seglen[parts] = Q.t.len;
        }
        parts++;
        total += 1 + Q.t.len;
        next_tok(&Q);
    }

    char *buf = (char *) Aalloc(P->A, (size_t) total + 1);
    int off = 0;
    int use = parts > 32 ? 32 : parts;
    for (int i = 0; i < use; i++) {
        if (i) buf[off++] = '.';
        memcpy(buf + off, segments[i], (size_t) seglen[i]);
        off += seglen[i];
    }
    buf[off] = 0;

    *P = Q;
    return buf;
}

static Ast *parse_primary(Parser *P) {
    if (accept(P, TK_LPAREN)) {
        skip_nl(P);
        Ast *e = parse_expr(P);
        skip_nl(P);
        expect(P, TK_RPAREN, ")");
        return e;
    }

    if (P->t.kind == TK_IDENT) {
        Token id = P->t;
        next_tok(P);
        char *qname = parse_qualified_name(P, id);

        if (accept(P, TK_LPAREN)) {
            Ast *n = newNode(P, ND_CALL);
            n->call.callee = qname;

            if (!accept(P, TK_RPAREN)) {
                for (;;) {
                    skip_nl(P);
                    Ast *a = parse_expr(P);
                    if (!a) return NULL;
                    list_push_ast(P->A, &n->call.args, a);
                    skip_nl(P);
                    if (accept(P, TK_COMMA)) continue;
                    expect(P, TK_RPAREN, ")");
                    break;
                }
            }
            return n;
        } else {
            Ast *n = newNode(P, ND_IDENT);
            n->ident.name = qname;
            return n;
        }
    }

    if (P->t.kind == TK_NUMBER) {
        Ast *n = newNode(P, ND_NUM);
        n->num = P->t.number;
        next_tok(P);
        return n;
    }

    if (P->t.kind == TK_STRING) {
        Ast *n = newNode(P, ND_STR);
        n->str = dupLex(P, &P->t);
        next_tok(P);
        return n;
    }

    if (accept(P, TK_LBRACK)) {
        Ast *n = newNode(P, ND_ARRAY);
        if (!accept(P, TK_RBRACK)) {
            for (;;) {
                skip_nl(P);
                Ast *a = parse_expr(P);
                if (!a) break;
                list_push_ast(P->A, &n->array.elems, a);
                skip_nl(P);
                if (accept(P, TK_COMMA)) continue;
                expect(P, TK_RBRACK, "]");
                break;
            }
        }
        return n;
    }

    return NULL;
}

static Ast *parse_unary(Parser *P) {
    if (accept(P, TK_MINUS)) {
        Ast *inner = parse_unary(P);
        if (!inner) return NULL;
        Ast *n = newNode(P, ND_NEG);
        n->un.expr = inner;
        return n;
    }
    return parse_primary(P);
}

static Ast *parse_term(Parser *P) {
    Ast *lhs = parse_unary(P);
    if (!lhs) return NULL;
    skip_nl(P);
    for (;;) {
        if (P->t.kind == TK_STAR) {
            next_tok(P);
            skip_nl(P);
            Ast *rhs = parse_unary(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, ND_MUL);
            n->mul.lhs = lhs;
            n->mul.rhs = rhs;
            lhs = n;
            skip_nl(P);
            continue;
        }
        if (P->t.kind == TK_SLASH) {
            next_tok(P);
            skip_nl(P);
            Ast *rhs = parse_unary(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, ND_DIV);
            n->div.lhs = lhs;
            n->div.rhs = rhs;
            lhs = n;
            skip_nl(P);
            continue;
        }
        break;
    }
    return lhs;
}

static Ast *parse_add(Parser *P) {
    Ast *lhs = parse_term(P);
    if (!lhs) return NULL;
    skip_nl(P);
    for (;;) {
        if (P->t.kind == TK_PLUS || P->t.kind == TK_MINUS) {
            TokenKind op = P->t.kind;
            next_tok(P);
            skip_nl(P);
            Ast *rhs = parse_term(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, (op == TK_PLUS) ? ND_ADD : ND_SUB);
            n->add.lhs = lhs;
            n->add.rhs = rhs;
            lhs = n;
            skip_nl(P);
            continue;
        }
        break;
    }
    return lhs;
}

static Ast *parse_compare(Parser *P, Ast *lhs) {
    if (!lhs) {
        lhs = parse_add(P);
        if (!lhs) return NULL;
    }
    skip_nl(P);
    for (;;) {
        NodeKind kind = 0;
        if (accept(P, TK_EQEQ)) kind = ND_EQ;
        else if (accept(P, TK_NEQ)) kind = ND_NEQ;
        else if (accept(P, TK_LT)) kind = ND_LT;
        else if (accept(P, TK_GT)) kind = ND_GT;
        else if (accept(P, TK_LTE)) kind = ND_LTE;
        else if (accept(P, TK_GTE)) kind = ND_GTE;
        else break;

        skip_nl(P);
        Ast *rhs = parse_add(P);
        if (!rhs) return NULL;

        Ast *n = newNode(P, kind);
        n->bin.lhs = lhs;
        n->bin.rhs = rhs;
        lhs = n;
        skip_nl(P);
    }
    return lhs;
}

static Ast *parse_expr(Parser *P) {
    if (P->t.kind == TK_IDENT) {
        Parser Q = *P;
        next_tok(&Q);
        skip_nl(&Q);
        if (Q.t.kind == TK_EQ) {
            Token id = P->t;
            next_tok(P);
            skip_nl(P);
            expect(P, TK_EQ, "=");
            skip_nl(P);
            Ast *rhs = parse_expr(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, ND_ASSIGN);
            n->assign.lhs = dupLex(P, &id);
            n->assign.rhs = rhs;
            return n;
        }
    }
    return parse_compare(P, NULL);
}

static Ast *parse_return(Parser *P) {
    Ast *n = newNode(P, ND_RETURN);
    expect(P, TK_RETURN, "return");
    skip_nl(P);
    if (P->t.kind != TK_SEMI) {
        for (;;) {
            skip_nl(P);
            Ast *e = parse_expr(P);
            if (!e) break;
            list_push_ast(P->A, &n->ret.exprs, e);
            skip_nl(P);
            if (accept(P, TK_COMMA)) continue;
            break;
        }
    }
    expect(P, TK_SEMI, ";");
    return n;
}

static Ast *parse_const(Parser *P) {
    Ast *n = newNode(P, ND_CONST);
    expect(P, TK_CONST, "const");
    Token name = P->t;
    expect(P, TK_IDENT, "identifier");
    n->const_.name = dupLex(P, &name);
    skip_nl(P);
    expect(P, TK_EQ, "=");
    skip_nl(P);
    n->const_.expr = parse_expr(P);
    expect(P, TK_SEMI, ";");
    return n;
}

static Ast *parse_block(Parser *P) {
    Ast *n = newNode(P, ND_BLOCK);
    skip_nl(P);
    if (!accept(P, TK_LBRACE)) {
        snprintf(P->err, sizeof(P->err), "expected {");
        P->hasErr = 1;
        P->errLine = P->t.line;
        P->errCol = P->t.col;
        return n;
    }
    skip_nl(P);
    while (P->t.kind != TK_RBRACE && P->t.kind != TK_EOF && !P->hasErr) {
        if (P->t.kind == TK_NEWLINE) {
            next_tok(P);
            continue;
        }
        Ast *s = parse_statement(P);
        if (!s) break;
        list_push_ast(P->A, &n->block.stmts, s);
        skip_nl(P);
    }
    expect(P, TK_RBRACE, "}");
    return n;
}

static Ast *parse_func(Parser *P) {
    Ast *n = newNode(P, ND_FUNC);
    Token name = P->t;
    expect(P, TK_IDENT, "identifier");
    n->func.name = dupLex(P, &name);
    expect(P, TK_LPAREN, "(");
    FParam *pars = NULL;
    int pc = 0, pcap = 0;
    if (!accept(P, TK_RPAREN)) {
        for (;;) {
            Token ttype = P->t;
            expect(P, TK_IDENT, "type");
            Token tname = P->t;
            expect(P, TK_IDENT, "param");
            if (pc >= pcap) {
                int nc = pcap ? pcap * 2 : 4;
                FParam *neu = (FParam *) Aalloc(P->A, sizeof(FParam) * nc);
                if (pars) memcpy(neu, pars, sizeof(FParam) * pc);
                pars = neu;
                pcap = nc;
            }
            pars[pc].type = dupLex(P, &ttype);
            pars[pc].name = dupLex(P, &tname);
            if (accept(P, TK_COMMA)) continue;
            expect(P, TK_RPAREN, ")");
            break;
        }
    }
    n->func.params = pars;
    n->func.pcount = pc;
    expect(P, TK_COLON, ":");
    if (!is_type_token(P->t.kind)) expect(P, TK_IDENT, "type");
    Token rt = P->t;
    next_tok(P);
    n->func.ret_type = dupLex(P, &rt);
    n->func.body = parse_block(P);
    return n;
}

static Ast *parse_for(Parser *P) {
    Ast *n = newNode(P, ND_FOR);
    expect(P, TK_FOR, "for");
    Token it = P->t;
    expect(P, TK_IDENT, "identifier");
    n->for_.iter = dupLex(P, &it);
    skip_nl(P);
    expect(P, TK_IN, "in");
    skip_nl(P);
    n->for_.from = parse_expr(P);
    if (!n->for_.from) return NULL;
    skip_nl(P);
    if (accept(P, TK_DOTDOT_EQ)) {
        n->for_.inclusive = 1;
    } else {
        expect(P, TK_DOTDOT, ".. or ..=");
        n->for_.inclusive = 0;
    }
    skip_nl(P);
    n->for_.to = parse_expr(P);
    if (!n->for_.to) return NULL;
    skip_nl(P);
    n->for_.body = parse_block(P);
    return n;
}

static Ast *parse_statement(Parser *P) {
    if (P->t.kind == TK_RETURN) return parse_return(P);
    if (P->t.kind == TK_FOR) return parse_for(P);
    if (P->t.kind == TK_CONST) return parse_const(P);
    if (P->t.kind == TK_IF) return parse_if(P);
    Ast *e = parse_expr(P);
    expect(P, TK_SEMI, ";");
    return e;
}

static Ast *parse_if(Parser *P) {
    Ast *n = newNode(P, ND_IF);
    expect(P, TK_IF, "if");
    expect(P, TK_LPAREN, "(");
    Ast *cond = parse_expr(P);
    expect(P, TK_RPAREN, ")");
    n->if_.cond = cond;
    skip_nl(P);
    n->if_.thenBranch = parse_block(P);
    skip_nl(P);
    if (accept(P, TK_ELSE)) {
        skip_nl(P);
        if (P->t.kind == TK_IF) n->if_.elseBranch = parse_if(P);
        else n->if_.elseBranch = parse_block(P);
    }
    return n;
}

static Param parse_typed_param(Parser *P) {
    Param p = (Param) {0};
    if (!is_type_token(P->t.kind)) return p;
    Token ttype = P->t;
    next_tok(P);
    skip_nl(P);
    Token tname = P->t;
    expect(P, TK_IDENT, "param");
    p.type = dupLex(P, &ttype);
    p.name = dupLex(P, &tname);
    skip_nl(P);
    if (accept(P, TK_EQ)) {
        skip_nl(P);
        p.value = parse_expr(P);
    }
    return p;
}

static Param parse_param(Parser *P) {
    Param p = (Param) {0};
    if (P->t.kind != TK_IDENT) return p;
    p.name = dupLex(P, &P->t);
    p.type = NULL;
    next_tok(P);
    if (accept(P, TK_EQ)) p.value = parse_unary(P);
    return p;
}

static NdPart parse_part_head(Parser *P, int isOverride) {
    NdPart nd = (NdPart) {0};
    nd.isOverride = isOverride ? 1 : 0;
    Token name = P->t;
    expect(P, TK_IDENT, "part name");
    nd.name = dupLex(P, &name);
    expect(P, TK_LPAREN, "(");
    Param *pars = NULL;
    int pc = 0, pcap = 0;
    if (!accept(P, TK_RPAREN)) {
        for (;;) {
            Param par = parse_typed_param(P);
            if (pc >= pcap) {
                int nc = pcap ? pcap * 2 : 4;
                Param *neu = (Param *) Aalloc(P->A, sizeof(Param) * nc);
                if (pars) memcpy(neu, pars, sizeof(Param) * pc);
                pars = neu;
                pcap = nc;
            }
            pars[pc++] = par;
            if (accept(P, TK_COMMA)) continue;
            expect(P, TK_RPAREN, ")");
            break;
        }
    }
    nd.params = pars;
    nd.pcount = pc;
    skip_annotation_to_lbrace(P);
    nd.body = parse_block(P);
    return nd;
}

static Ast *parse_mesh(Parser *P) {
    Ast *n = newNode(P, ND_MESH);
    Token nm = P->t;
    expect(P, TK_IDENT, "mesh name");
    n->mesh.name = dupLex(P, &nm);
    if (accept(P, TK_COLON)) {
        Token p = P->t;
        expect(P, TK_IDENT, "parent name");
        n->mesh.parent = dupLex(P, &p);
    }
    skip_nl(P);
    expect(P, TK_LBRACE, "{");
    for (;;) {
        if (P->t.kind == TK_RBRACE) {
            next_tok(P);
            break;
        }
        if (P->t.kind == TK_EOF || P->hasErr) break;
        if (P->t.kind == TK_NEWLINE) {
            next_tok(P);
            continue;
        }
        if (P->t.kind == TK_PART || P->t.kind == TK_OVERRIDE) {
            int ov = (P->t.kind == TK_OVERRIDE);
            next_tok(P);
            Ast *it = newNode(P, ND_PART);
            it->part = parse_part_head(P, ov);
            list_push_ast(P->A, &n->mesh.items, it);
            continue;
        }
        if (P->t.kind == TK_CREATE) {
            next_tok(P);
            Ast *it = newNode(P, ND_CREATE);
            expect(P, TK_LPAREN, "(");
            Param *pars = NULL;
            int pc = 0, pcap = 0;
            if (!accept(P, TK_RPAREN)) {
                for (;;) {
                    Param par = parse_param(P);
                    if (pc >= pcap) {
                        int nc = pcap ? pcap * 2 : 4;
                        Param *neu = (Param *) Aalloc(P->A, sizeof(Param) * nc);
                        if (pars) memcpy(neu, pars, sizeof(Param) * pc);
                        pars = neu;
                        pcap = nc;
                    }
                    pars[pc++] = par;
                    if (accept(P, TK_COMMA)) continue;
                    expect(P, TK_RPAREN, ")");
                    break;
                }
            }
            it->create.params = pars;
            it->create.pcount = pc;
            skip_annotation_to_lbrace(P);
            it->create.body = parse_block(P);
            list_push_ast(P->A, &n->mesh.items, it);
            continue;
        }
        if (P->t.kind == TK_CONST) {
            Ast *c = parse_const(P);
            list_push_ast(P->A, &n->mesh.items, c);
            continue;
        }
        if (is_func_decl(P)) {
            Ast *f = parse_func(P);
            list_push_ast(P->A, &n->mesh.items, f);
            continue;
        }
        next_tok(P);
    }
    return n;
}

AstProgram parse_program(const char *src, TopoArena *A, char err[256], int *line, int *col) {
    Parser P = (Parser) {0};
    P.A = A;
    P.file = g_parse_filename;
    lex_init(&P.L, src);
    next_tok(&P);
    AstProgram pr = (AstProgram) {0};
    while (P.t.kind != TK_EOF && !P.hasErr) {
        if (P.t.kind == TK_NEWLINE) {
            next_tok(&P);
            continue;
        }
        if (P.t.kind == TK_IMPORT) {
            next_tok(&P);
            Token s = P.t;
            expect(&P, TK_STRING, "\"file.tl\"");
            Ast *imp = newNode(&P, ND_IMPORT);
            imp->import_.path = dupLex(&P, &s);
            if (pr.gcount >= pr.gcap) {
                int nc = pr.gcap ? pr.gcap * 2 : 8;
                Ast **neu = (Ast **) arena_alloc(A, sizeof(Ast *) * nc, 8);
                if (pr.globals) memcpy(neu, pr.globals, sizeof(Ast *) * pr.gcount);
                pr.globals = neu;
                pr.gcap = nc;
            }
            pr.globals[pr.gcount++] = imp;
            expect(&P, TK_SEMI, ";");
            continue;
        }
        if (P.t.kind == TK_CONST) {
            Ast *c = parse_const(&P);
            if (pr.gcount >= pr.gcap) {
                int nc = pr.gcap ? pr.gcap * 2 : 8;
                Ast **neu = (Ast **) arena_alloc(A, sizeof(Ast *) * nc, 8);
                if (pr.globals) memcpy(neu, pr.globals, sizeof(Ast *) * pr.gcount);
                pr.globals = neu;
                pr.gcap = nc;
            }
            pr.globals[pr.gcount++] = c;
            continue;
        }
        if (P.t.kind == TK_MESH) {
            next_tok(&P);
            Ast *m = parse_mesh(&P);
            if (!m) {
                P.hasErr = 1;
                break;
            }
            if (pr.count >= pr.cap) {
                int nc = pr.cap ? pr.cap * 2 : 8;
                Ast **neu = (Ast **) arena_alloc(A, sizeof(Ast *) * nc, 8);
                if (pr.meshes) memcpy(neu, pr.meshes, sizeof(Ast *) * pr.count);
                pr.meshes = neu;
                pr.cap = nc;
            }
            pr.meshes[pr.count++] = m;
            continue;
        }
        next_tok(&P);
    }
    if (P.hasErr) {
        if (err) strsncpy(err, P.err, 256);
        if (line) *line = P.errLine;
        if (col) *col = P.errCol;
    }
    return pr;
}
