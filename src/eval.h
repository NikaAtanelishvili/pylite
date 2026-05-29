// eval.h
#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include "value.h"
#include "env.h"

typedef struct {
    Env  *globals;      // top-level scope, persists for the whole program
    int   returning;    // 1 when a return statement fires
    Value return_val;   // the value being returned
} Interpreter;

void  interp_init(Interpreter *interp);
void  interp_run (Interpreter *interp, ASTNode *program);

#endif