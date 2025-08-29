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
    char err[256];
    int errLine, errCol;
    int hasErr;
} Parser;

static void *Aalloc(TopoArena *A, size_t sz) { return arena_alloc(A, sz, 8); }

static Ast *newNode(Parser *P, NodeKind k) {
    Ast *n = (Ast *) Aalloc(P->A, sizeof(Ast));
    memset(n, 0, sizeof(*n));
    n->kind = k;
    n->line = P->t.line;
    n->col = P->t.col;
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

static char *dupLex(Parser *P, const Token *t) {
    char *s = (char *) Aalloc(P->A, (size_t) t->len + 1);
    memcpy(s, t->lexeme, (size_t) t->len);
    s[t->len] = 0;
    return s;
}

static void skip_nl(Parser *P) { while (P->t.kind == TK_NEWLINE) next_tok(P); }

static void skip_annotation_to_lbrace(Parser *P) {
    if (!accept(P, TK_COLON)) return;
    for (;;) {
        if (P->t.kind == TK_LBRACE || P->t.kind == TK_EOF) break;
        if (P->t.kind == TK_NEWLINE || P->t.kind == TK_COMMA || P->t.kind == TK_IDENT) {
            next_tok(P);
            continue;
        }
        next_tok(P);
    }
}

static Ast *parse_expr(Parser *P);

static Ast *parse_statement(Parser *P);

static Ast *parse_primary(Parser *P) {
    if (P->t.kind == TK_IDENT) {
        Token id = P->t;
        next_tok(P);
        if (accept(P, TK_LPAREN)) {
            Ast *n = newNode(P, ND_CALL);
            n->call.callee = dupLex(P, &id);
            if (!accept(P, TK_RPAREN)) {
                for (;;) {
                    Ast *a = parse_expr(P);
                    if (!a) return NULL;
                    list_push_ast(P->A, &n->call.args, a);
                    if (accept(P, TK_COMMA)) continue;
                    expect(P, TK_RPAREN, ")");
                    break;
                }
            }
            return n;
        } else {
            Ast *n = newNode(P, ND_IDENT);
            n->ident.name = dupLex(P, &id);
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
                Ast *a = parse_expr(P);
                if (!a) break;
                list_push_ast(P->A, &n->array.elems, a);
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
        if (P->t.kind == TK_PLUS) {
            next_tok(P);
            skip_nl(P);
            Ast *rhs = parse_term(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, ND_ADD);
            n->add.lhs = lhs;
            n->add.rhs = rhs;
            lhs = n;
            skip_nl(P);
            continue;
        }
        if (P->t.kind == TK_MINUS) {
            next_tok(P);
            skip_nl(P);
            Ast *rhs = parse_term(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, ND_SUB);
            n->sub.lhs = lhs;
            n->sub.rhs = rhs;
            lhs = n;
            skip_nl(P);
            continue;
        }
        break;
    }
    return lhs;
}

static Ast *parse_expr(Parser *P) {
    if (P->t.kind == TK_IDENT) {
        Token id = P->t;
        next_tok(P);
        skip_nl(P);
        if (accept(P, TK_EQ)) {
            skip_nl(P);
            Ast *rhs = parse_expr(P);
            if (!rhs) return NULL;
            Ast *n = newNode(P, ND_ASSIGN);
            n->assign.lhs = dupLex(P, &id);
            n->assign.rhs = rhs;
            return n;
        } else {
            Ast *acc = NULL;
            if (accept(P, TK_LPAREN)) {
                Ast *call = newNode(P, ND_CALL);
                call->call.callee = dupLex(P, &id);
                if (!accept(P, TK_RPAREN)) {
                    for (;;) {
                        Ast *a = parse_expr(P);
                        if (!a) return NULL;
                        list_push_ast(P->A, &call->call.args, a);
                        if (accept(P, TK_COMMA)) continue;
                        expect(P, TK_RPAREN, ")");
                        break;
                    }
                }
                acc = call;
            } else {
                Ast *ident = newNode(P, ND_IDENT);
                ident->ident.name = dupLex(P, &id);
                acc = ident;
            }
            skip_nl(P);
            for (;;) {
                if (P->t.kind == TK_PLUS) {
                    next_tok(P);
                    skip_nl(P);
                    Ast *rhs = parse_term(P);
                    if (!rhs) return NULL;
                    Ast *n = newNode(P, ND_ADD);
                    n->add.lhs = acc;
                    n->add.rhs = rhs;
                    acc = n;
                    skip_nl(P);
                    continue;
                }
                if (P->t.kind == TK_MINUS) {
                    next_tok(P);
                    skip_nl(P);
                    Ast *rhs = parse_term(P);
                    if (!rhs) return NULL;
                    Ast *n = newNode(P, ND_SUB);
                    n->sub.lhs = acc;
                    n->sub.rhs = rhs;
                    acc = n;
                    skip_nl(P);
                    continue;
                }
                break;
            }
            return acc;
        }
    }
    return parse_add(P);
}

static Ast *parse_return(Parser *P) {
    Ast *n = newNode(P, ND_RETURN);
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

static Ast *parse_statement(Parser *P) {
    if (P->t.kind == TK_RETURN) {
        next_tok(P);
        return parse_return(P);
    }
    Ast *e = parse_expr(P);
    expect(P, TK_SEMI, ";");
    return e;
}

static Param parse_param(Parser *P) {
    Param p = (Param) {0};
    if (P->t.kind != TK_IDENT) return p;
    p.name = dupLex(P, &P->t);
    next_tok(P);
    if (accept(P, TK_EQ)) {
        Ast *lit = parse_unary(P);
        p.value = lit;
    }
    return p;
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
    while (P->t.kind != TK_RBRACE && P->t.kind != TK_EOF && !P->hasErr) {
        if (P->t.kind == TK_NEWLINE) {
            next_tok(P);
            continue;
        }
        Ast *s = parse_statement(P);
        if (!s) break;
        list_push_ast(P->A, &n->block.stmts, s);
    }
    skip_nl(P);
    expect(P, TK_RBRACE, "}");
    return n;
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
        if (P->t.kind == TK_IMPORT) {
            next_tok(P);
            Token s = P->t;
            expect(P, TK_STRING, "\"file.tl\"");
            Ast *imp = newNode(P, ND_IMPORT);
            imp->import_.path = dupLex(P, &s);
            list_push_ast(P->A, &n->mesh.items, imp);
            expect(P, TK_SEMI, ";");
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
        next_tok(P);
    }
    return n;
}

AstProgram parse_program(const char *src, TopoArena *A, char err[256], int *line, int *col) {
    Parser P = (Parser) {0};
    P.A = A;
    lex_init(&P.L, src);
    next_tok(&P);
    AstProgram pr = (AstProgram) {0};
    while (P.t.kind != TK_EOF && !P.hasErr) {
        if (P.t.kind == TK_NEWLINE) {
            next_tok(&P);
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
        } else {
            next_tok(&P);
        }
    }
    if (P.hasErr) {
        if (err) strsncpy(err, P.err, 256);
        if (line) *line = P.errLine;
        if (col) *col = P.errCol;
    }
    return pr;
}
