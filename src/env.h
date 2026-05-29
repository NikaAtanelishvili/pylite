// env.h
#ifndef ENV_H
#define ENV_H

#include "value.h"

#define ENV_MAX_VARS 64

typedef struct Env Env;
struct Env {
    char  names[ENV_MAX_VARS][64];
    Value values[ENV_MAX_VARS];
    int   count;
    Env  *parent;   // enclosing scope — NULL at global level
};

Env  *env_new(Env *parent);
void  env_free(Env *env);

// Search current scope then all parents. Error if not found.
Value env_get(Env *env, const char *name, int line);

// Update if found anywhere in chain, otherwise create in current scope.
void  env_set(Env *env, const char *name, Value val);

// Always write to current scope (used for function parameters).
void  env_set_local(Env *env, const char *name, Value val);

#endif