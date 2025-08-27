#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include "intrinsics.h"
#include "arena.h"
#include <stdbool.h>

typedef struct {
    int hasReturn;
    Value ret;
} EvalResult;

bool eval_block_to_value(Ast *block, TopoArena *A, EvalResult *out, char err[256]);

#endif
