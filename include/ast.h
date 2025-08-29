#ifndef AST_H
#define AST_H

#include <stdbool.h>

typedef struct Ast Ast;

typedef struct AstList {
    Ast **data;
    int count, cap;
} AstList;

typedef enum {
    ND_PROG, ND_MESH, ND_PART, ND_CREATE,
    ND_BLOCK,
    ND_ASSIGN, ND_CALL, ND_IDENT, ND_NUM, ND_STR,
    ND_RETURN, ND_IMPORT, ND_ARRAY, ND_ADD, ND_NEG,
    ND_SUB, ND_MUL, ND_DIV
} NodeKind;

typedef struct {
    char *name;
    Ast *value;
} Param;

typedef struct {
    char *name;
    char *parent;
    AstList items;
} NdMesh;

typedef struct {
    char *name;
    Param *params;
    int pcount;
    Ast *body;
    bool isOverride;
} NdPart;

typedef struct {
    Param *params;
    int pcount;
    Ast *body;
} NdCreate;

typedef struct {
    char *name;
} NdIdent;

typedef struct {
    char *callee;
    AstList args;
} NdCall;

typedef struct {
    AstList elems;
} NdArray;

typedef struct Ast {
    NodeKind kind;
    int line, col;
    union {
        NdMesh mesh;
        NdPart part;
        NdCreate create;
        NdIdent ident;
        NdCall call;
        NdArray array;
        double num;
        char *str;
        struct {
            Ast *lhs;
            Ast *rhs;
        } add;
        struct {
            Ast *lhs;
            Ast *rhs;
        } sub;
        struct {
            Ast *lhs;
            Ast *rhs;
        } mul;
        struct {
            Ast *lhs;
            Ast *rhs;
        } div;
        struct {
            Ast *expr;
        } un;
        struct {
            char *lhs;
            Ast *rhs;
        } assign;
        struct {
            AstList stmts;
        } block;
        struct {
            AstList exprs;
        } ret;
        struct {
            char *path;
        } import_;
    };
} Ast;

typedef struct {
    Ast **meshes;
    int count, cap;
} AstProgram;

#endif
