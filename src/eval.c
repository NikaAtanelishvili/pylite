// eval.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "eval.h"

// value constructors

Value make_number(double n) {
    return (Value){ .type = VAL_NUMBER, .as.num     = n };
}
Value make_bool(int b) {
    return (Value){ .type = VAL_BOOL,   .as.boolean = b != 0 };
}
Value make_null(void) {
    return (Value){ .type = VAL_NULL };
}
Value make_string(const char *s) {
    Value v = { .type = VAL_STRING };
    v.as.str = malloc(strlen(s) + 1);
    strcpy(v.as.str, s);
    return v;
}
Value make_func(const char *name, char params[][64], int nparams, ASTNode *body) {
    FuncDef *f = malloc(sizeof(FuncDef));
    strncpy(f->name, name, 63);
    f->param_count = nparams;
    for (int i = 0; i < nparams; i++)
        strncpy(f->params[i], params[i], 63);
    f->body = body;
    return (Value){ .type = VAL_FUNC, .as.func = f };
}

// value utilities 

int val_is_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:   return v.as.boolean;
        case VAL_NUMBER: return v.as.num != 0.0;
        case VAL_STRING: return v.as.str && v.as.str[0] != '\0';
        case VAL_NULL:   return 0;
        case VAL_FUNC:   return 1;
    }
    return 0;
}

void val_to_cstr(Value v, char *buf, int size) {
    switch (v.type) {
        case VAL_NUMBER:
            // Show whole numbers without decimal point: 3 not 3.0
            if (v.as.num == (long long)v.as.num)
                snprintf(buf, size, "%lld", (long long)v.as.num);
            else
                snprintf(buf, size, "%g",   v.as.num);
            break;
        case VAL_STRING:  snprintf(buf, size, "%s",   v.as.str ? v.as.str : ""); break;
        case VAL_BOOL:    snprintf(buf, size, "%s",   v.as.boolean ? "true" : "false"); break;
        case VAL_NULL:    snprintf(buf, size, "null"); break;
        case VAL_FUNC:    snprintf(buf, size, "<func %s>", v.as.func->name); break;
    }
}

void val_print(Value v) {
    char buf[512];
    val_to_cstr(v, buf, sizeof(buf));
    printf("%s", buf);
}

// forward declarations

static Value eval_node(Interpreter *interp, ASTNode *node, Env *env);

// statement helpers

// Run a linked list of statement nodes, stopping early on `return`.
// This is the core loop that every block (if body, while body, func body) uses.
static void eval_stmt_list(Interpreter *interp, ASTNode *stmts, Env *env) {
    for (ASTNode *s = stmts; s && !interp->returning; s = s->next)
        eval_node(interp, s, env);
}

// statement evaluators 

static Value eval_assign(Interpreter *interp, ASTNode *node, Env *env) {
    Value val = eval_node(interp, node->as.assign.value, env);
    env_set(env, node->as.assign.name, val);
    return val;
}

static Value eval_if(Interpreter *interp, ASTNode *node, Env *env) {
    Value cond = eval_node(interp, node->as.if_stmt.cond, env);

    if (val_is_truthy(cond)) {
        // Each branch gets a child scope so its locals don't leak out
        Env *branch = env_new(env);
        eval_stmt_list(interp, node->as.if_stmt.then_branch, branch);
        env_free(branch);
    } else if (node->as.if_stmt.else_branch) {
        Env *branch = env_new(env);
        eval_stmt_list(interp, node->as.if_stmt.else_branch, branch);
        env_free(branch);
    }
    return make_null();
}

static Value eval_while(Interpreter *interp, ASTNode *node, Env *env) {
    while (!interp->returning) {
        Value cond = eval_node(interp, node->as.while_stmt.cond, env);
        if (!val_is_truthy(cond)) break;

        Env *loop = env_new(env);
        eval_stmt_list(interp, node->as.while_stmt.body, loop);
        env_free(loop);
    }
    return make_null();
}

// func name(a, b) { } — just stores the function as a value in the env.
// The body is NOT run here. It runs later when the function is called.
static Value eval_func_decl(Interpreter *interp, ASTNode *node, Env *env) {
    Value func = make_func(
        node->as.func_decl.name,
        node->as.func_decl.params,
        node->as.func_decl.param_count,
        node->as.func_decl.body
    );
    env_set(env, node->as.func_decl.name, func);
    return make_null();
}

// return [expr] — sets the returning flag and stores the value.
// eval_stmt_list sees the flag and stops immediately, unwinding all nested calls.
static Value eval_return(Interpreter *interp, ASTNode *node, Env *env) {
    interp->return_val = node->as.ret.value
        ? eval_node(interp, node->as.ret.value, env)
        : make_null();
    interp->returning = 1;
    return interp->return_val;
}

static Value eval_print(Interpreter *interp, ASTNode *node, Env *env) {
    ASTNode *arg   = node->as.print.args;
    int      first = 1;
    while (arg) {
        if (!first) printf(" ");
        val_print(eval_node(interp, arg, env));
        first = 0;
        arg = arg->next;
    }
    printf("\n");
    return make_null();
}

// expression evaluators 

static Value eval_binary(Interpreter *interp, ASTNode *node, Env *env) {
    TokenType op = node->as.binary.op;

    // Short-circuit: evaluate right side ONLY if needed
    if (op == TOK_AND) {
        Value l = eval_node(interp, node->as.binary.left, env);
        if (!val_is_truthy(l)) return make_bool(0);
        return make_bool(val_is_truthy(eval_node(interp, node->as.binary.right, env)));
    }
    if (op == TOK_OR) {
        Value l = eval_node(interp, node->as.binary.left, env);
        if (val_is_truthy(l))  return make_bool(1);
        return make_bool(val_is_truthy(eval_node(interp, node->as.binary.right, env)));
    }

    // All other operators: evaluate both sides eagerly
    Value L = eval_node(interp, node->as.binary.left,  env);
    Value R = eval_node(interp, node->as.binary.right, env);

    switch (op) {

        case TOK_PLUS:
            if (L.type == VAL_NUMBER && R.type == VAL_NUMBER)
                return make_number(L.as.num + R.as.num);
            if (L.type == VAL_STRING && R.type == VAL_STRING) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s%s", L.as.str, R.as.str);
                return make_string(buf);
            }
            fprintf(stderr, "line %d: '+' needs two numbers or two strings\n", node->line);
            return make_null();

        case TOK_MINUS:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '-' needs numbers\n", node->line); return make_null();
            }
            return make_number(L.as.num - R.as.num);

        case TOK_STAR:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '*' needs numbers\n", node->line); return make_null();
            }
            return make_number(L.as.num * R.as.num);

        case TOK_SLASH:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '/' needs numbers\n", node->line); return make_null();
            }
            if (R.as.num == 0.0) {
                fprintf(stderr, "line %d: division by zero\n", node->line); return make_null();
            }
            return make_number(L.as.num / R.as.num);

        case TOK_PERCENT:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '%%' needs numbers\n", node->line); return make_null();
            }
            if (R.as.num == 0.0) {
                fprintf(stderr, "line %d: modulo by zero\n", node->line); return make_null();
            }
            return make_number(fmod(L.as.num, R.as.num));

        case TOK_EQ:
            if (L.type != R.type)          return make_bool(0);
            if (L.type == VAL_NUMBER)      return make_bool(L.as.num == R.as.num);
            if (L.type == VAL_STRING)      return make_bool(strcmp(L.as.str, R.as.str) == 0);
            if (L.type == VAL_BOOL)        return make_bool(L.as.boolean == R.as.boolean);
            if (L.type == VAL_NULL)        return make_bool(1);
            return make_bool(0);

        case TOK_NEQ:
            if (L.type != R.type)          return make_bool(1);
            if (L.type == VAL_NUMBER)      return make_bool(L.as.num != R.as.num);
            if (L.type == VAL_STRING)      return make_bool(strcmp(L.as.str, R.as.str) != 0);
            if (L.type == VAL_BOOL)        return make_bool(L.as.boolean != R.as.boolean);
            if (L.type == VAL_NULL)        return make_bool(0);
            return make_bool(1);

        case TOK_LT:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '<' needs numbers\n", node->line); return make_null();
            }
            return make_bool(L.as.num < R.as.num);

        case TOK_GT:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '>' needs numbers\n", node->line); return make_null();
            }
            return make_bool(L.as.num > R.as.num);

        case TOK_LTE:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '<=' needs numbers\n", node->line); return make_null();
            }
            return make_bool(L.as.num <= R.as.num);

        case TOK_GTE:
            if (L.type != VAL_NUMBER || R.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: '>=' needs numbers\n", node->line); return make_null();
            }
            return make_bool(L.as.num >= R.as.num);

        default:
            fprintf(stderr, "line %d: unknown binary operator\n", node->line);
            return make_null();
    }
}

static Value eval_unary(Interpreter *interp, ASTNode *node, Env *env) {
    Value operand = eval_node(interp, node->as.unary.operand, env);
    switch (node->as.unary.op) {
        case TOK_MINUS:
            if (operand.type != VAL_NUMBER) {
                fprintf(stderr, "line %d: unary '-' needs a number\n", node->line);
                return make_null();
            }
            return make_number(-operand.as.num);
        case TOK_NOT:
            return make_bool(!val_is_truthy(operand));
        default:
            return make_null();
    }
}

static Value eval_call(Interpreter *interp, ASTNode *node, Env *env) {
    // 1. Look up the function in the environment
    Value func_val = env_get(env, node->as.call.name, node->line);
    if (func_val.type != VAL_FUNC) {
        fprintf(stderr, "line %d: '%s' is not a function\n",
                node->line, node->as.call.name);
        return make_null();
    }
    FuncDef *func = func_val.as.func;

    // 2. Check argument count
    if (node->as.call.arg_count != func->param_count) {
        fprintf(stderr, "line %d: '%s' expects %d arg(s), got %d\n",
                node->line, func->name,
                func->param_count, node->as.call.arg_count);
        return make_null();
    }

    // 3. Create a fresh scope.
    //    Parent is globals, not the caller — functions see globals and their
    //    own locals, but NOT the caller's local variables. Clean scoping.
    Env *func_env = env_new(interp->globals);

    // 4. Evaluate arguments in the CALLER'S scope, bind in function's scope
    ASTNode *arg = node->as.call.args;
    for (int i = 0; i < func->param_count; i++) {
        Value v = eval_node(interp, arg, env);   // caller's env — important!
        env_set_local(func_env, func->params[i], v);
        arg = arg->next;
    }

    // 5. Run the body
    eval_stmt_list(interp, func->body, func_env);

    // 6. Collect return value and clear the flag so the caller keeps running
    Value result = make_null();
    if (interp->returning) {
        result = interp->return_val;
        interp->returning = 0;  // ← critical: only the direct caller clears this
    }

    env_free(func_env);
    return result;
}

// main dispatch

static Value eval_node(Interpreter *interp, ASTNode *node, Env *env) {
    if (!node) return make_null();

    switch (node->type) {

        // Statements
        case NODE_PROGRAM:
            eval_stmt_list(interp, node->as.program.stmts, env);
            return make_null();
        case NODE_ASSIGN:    return eval_assign(interp, node, env);
        case NODE_IF:        return eval_if(interp, node, env);
        case NODE_WHILE:     return eval_while(interp, node, env);
        case NODE_FUNC_DECL: return eval_func_decl(interp, node, env);
        case NODE_RETURN:    return eval_return(interp, node, env);
        case NODE_PRINT:     return eval_print(interp, node, env);
        case NODE_EXPR_STMT: return eval_node(interp, node->as.expr_stmt.expr, env);

        // Expressions
        case NODE_BINARY:    return eval_binary(interp, node, env);
        case NODE_UNARY:     return eval_unary(interp, node, env);
        case NODE_CALL:      return eval_call(interp, node, env);
        case NODE_IDENT:     return env_get(env, node->as.ident.name, node->line);
        case NODE_NUMBER:    return make_number(node->as.number.value);
        case NODE_STRING:    return make_string(node->as.string.value);
        case NODE_BOOL:      return make_bool(node->as.boolean.value);

        default:
            fprintf(stderr, "eval: unhandled node type %d\n", node->type);
            return make_null();
    }
}

// public api 

void interp_init(Interpreter *interp) {
    interp->globals    = env_new(NULL);
    interp->returning  = 0;
    interp->return_val = make_null();
}

void interp_run(Interpreter *interp, ASTNode *program) {
    eval_node(interp, program, interp->globals);
}