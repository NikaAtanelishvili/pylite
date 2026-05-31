// env.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env.h"

Env *env_new(Env *parent) {
    Env *e  = calloc(1, sizeof(Env));
    e->parent = parent;
    e->is_func_scope = 0; 
    return e;
}

// Used only by eval_call
Env *env_new_func(Env *parent) {
    Env *e = env_new(parent);
    e->is_func_scope = 1;    // function scopes are opaque to env_set
    return e;
}

void env_free(Env *env) {
    // Strings inside values are not freed here — acceptable for a class project.
    // A production interpreter would use reference counting or a GC.
    free(env);
}

Value env_get(Env *env, const char *name, int line) {
    for (Env *e = env; e != NULL; e = e->parent)
        for (int i = 0; i < e->count; i++)
            if (strcmp(e->names[i], name) == 0)
                return e->values[i];

    fprintf(stderr, "line %d: undefined variable '%s'\n", line, name);
    return make_null();
}

void env_set_local(Env *env, const char *name, Value val) {
    // Update existing slot in current scope
    for (int i = 0; i < env->count; i++)
        if (strcmp(env->names[i], name) == 0) {
            env->values[i] = val;
            return;
        }
    // Create new slot
    if (env->count >= ENV_MAX_VARS) {
        fprintf(stderr, "error: too many variables in scope (max %d)\n", ENV_MAX_VARS);
        return;
    }
    strncpy(env->names[env->count], name, 63);
    env->values[env->count] = val;
    env->count++;
}

void env_set(Env *env, const char *name, Value val) {
    for (Env *e = env; e != NULL; e = e->parent) {
        for (int i = 0; i < e->count; i++) {
            if (strcmp(e->names[i], name) == 0) {
                e->values[i] = val;
                return;
            }
        }
        if (e->is_func_scope) break; 
    }
    env_set_local(env, name, val);
}