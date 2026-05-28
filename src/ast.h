// ast.h
#ifndef AST_H
#define AST_H

#include "token.h"

/*C unions don't inherently know which variant they 
are currently holding, evaluator will read this type field first */
typedef enum {
    // Statements
    NODE_PROGRAM,     // root node — holds the full stmt list
    NODE_ASSIGN,      // x = <expr>
    NODE_IF,          // if <expr> { } else { }
    NODE_WHILE,       // while <expr> { }
    NODE_FUNC_DECL,   // func name(params) { }
    NODE_RETURN,      // return <expr>
    NODE_PRINT,       // print(args)
    NODE_EXPR_STMT,   // a bare expression used as a statement
    // Expressions
    NODE_BINARY,      // left OP right
    NODE_UNARY,       // -x  or  not x
    NODE_CALL,        // name(args)
    NODE_IDENT,       // a variable name
    NODE_NUMBER,      // 42  or  3.14
    NODE_STRING,      // "hello"
    NODE_BOOL         // true / false
} NodeType;

// Forward declaration so the struct can reference itself
typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeType type;
    int      line;    // source line: for error messages in the evaluator
    ASTNode *next;    // intrusive linked list: chains stmts and arg lists

    union {
        // NODE_PROGRAM
        struct {
            ASTNode *stmts;           // head of statement linked list
        } program;

        // NODE_ASSIGN
        struct {
            char     name[64];        // variable being assigned to
            ASTNode *value;           // right-hand side expression
        } assign;

        // NODE_IF
        struct {
            ASTNode *cond;            // condition expression
            ASTNode *then_branch;     // body if true
            ASTNode *else_branch;     // body if false — NULL when no else
        } if_stmt;

        // NODE_WHILE
        struct {
            ASTNode *cond;
            ASTNode *body;
        } while_stmt;

        // NODE_FUNC_DECL
        struct {
            char    name[64];
            char    params[8][64];    // up to 8 parameter names
            int     param_count;
            ASTNode *body;
        } func_decl;

        // NODE_RETURN
        struct {
            ASTNode *value;           // NULL for bare `return`
        } ret;

        // NODE_PRINT
        struct {
            ASTNode *args;            // linked list of arg expressions
            int      arg_count;
        } print;

        // NODE_EXPR_STMT
        struct {
            ASTNode *expr;
        } expr_stmt;

        // NODE_BINARY
        struct {
            TokenType op;             // TOK_PLUS, TOK_EQ, TOK_AND, etc.
            ASTNode  *left;
            ASTNode  *right;
        } binary;

        // NODE_UNARY
        struct {
            TokenType op;             // TOK_MINUS or TOK_NOT
            ASTNode  *operand;
        } unary;

        // NODE_CALL
        struct {
            char    name[64];
            ASTNode *args;            // linked list
            int      arg_count;
        } call;

        // NODE_IDENT
        struct {
            char name[64];
        } ident;

        // NODE_NUMBER
        struct {
            double value;
        } number;

        // NODE_STRING
        struct {
            char value[256];
        } string;

        // NODE_BOOL
        struct {
            int value;                // 1 = true, 0 = false
        } boolean;

    } as;   // "as" — read node->as.binary.left, node->as.number.value, etc.
};

// Allocate and zero a new node (calloc so all pointers start NULL)
ASTNode *ast_new_node(NodeType type, int line);

// Recursively free an AST
void ast_free(ASTNode *node);

// Debug: print the tree (useful while building)
void ast_print(ASTNode *node, int indent);

#endif