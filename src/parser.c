// parser.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// node allocation

ASTNode *ast_new_node(NodeType type, int line) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    if (!n) { fprintf(stderr, "out of memory\n"); exit(1); }
    n->type = type;
    n->line = line;
    return n;
}

// low-level parser primitives

// Move forward one token
static void advance(Parser *p) {
    p->previous = p->current;
    p->current  = lexer_next(p->lexer);
}

// Is the current token this type?
static int check(Parser *p, TokenType t) {
    return p->current.type == t;
}

// If the current token matches, consume it and return 1. Otherwise return 0.
static int match(Parser *p, TokenType t) {
    if (!check(p, t)) return 0;
    advance(p);
    return 1;
}

// Consume current token and return it. Print an error if type doesn't match.
static Token expect(Parser *p, TokenType t, const char *what) {
    if (!check(p, t)) {
        fprintf(stderr, "line %d: expected %s but got '%s'\n",
                p->current.line, what, p->current.lexeme);
        p->had_error = 1;
    }
    advance(p);
    return p->previous;
}

// Skip any number of consecutive newlines (blank lines between statements)
static void skip_newlines(Parser *p) {
    while (check(p, TOK_NEWLINE)) advance(p);
}

// Consume the newline that ends a statement.
// Tolerant: also accepts EOF (last line of file) and } (end of block).
static void expect_newline(Parser *p) {
    if (check(p, TOK_EOF) || check(p, TOK_RBRACE)) return;
    if (!match(p, TOK_NEWLINE)) {
        fprintf(stderr, "line %d: expected newline after statement, got '%s'\n",
                p->current.line, p->current.lexeme);
        p->had_error = 1;
    }
}

// forward declarations (functions call each other recursively)

static ASTNode *parse_stmt(Parser *p);
static ASTNode *parse_expr(Parser *p);
static ASTNode *parse_or_expr(Parser *p);
static ASTNode *parse_and_expr(Parser *p);
static ASTNode *parse_not_expr(Parser *p);
static ASTNode *parse_cmp_expr(Parser *p);
static ASTNode *parse_add_expr(Parser *p);
static ASTNode *parse_mul_expr(Parser *p);
static ASTNode *parse_unary_expr(Parser *p);
static ASTNode *parse_primary(Parser *p);

// statement list

// Parse a { block } — caller already consumed the {
// Stops at } or EOF. Returns the head of a linked list of statement nodes.
static ASTNode *parse_block(Parser *p) {
    skip_newlines(p);
    ASTNode *head = NULL, *tail = NULL;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ASTNode *s = parse_stmt(p);
        if (!s) { skip_newlines(p); continue; }
        if (!head) head = tail = s;
        else       { tail->next = s; tail = s; }
        skip_newlines(p);
    }

    expect(p, TOK_RBRACE, "}");
    return head;
}

// individual statement parsers

// x = <expr>
static ASTNode *parse_assign_stmt(Parser *p) {
    Token name_tok = p->current;
    advance(p);  // consume IDENT
    advance(p);  // consume =

    ASTNode *node  = ast_new_node(NODE_ASSIGN, name_tok.line);
    strncpy(node->as.assign.name, name_tok.lexeme, 63);
    node->as.assign.value = parse_expr(p);
    expect_newline(p);
    return node;
}

// if <expr> { } [else { }]
static ASTNode *parse_if_stmt(Parser *p) {
    int line = p->current.line;
    advance(p);  // consume 'if'

    ASTNode *node     = ast_new_node(NODE_IF, line);
    node->as.if_stmt.cond = parse_expr(p);

    expect(p, TOK_LBRACE, "{");
    node->as.if_stmt.then_branch = parse_block(p);

    skip_newlines(p);
    if (match(p, TOK_ELSE)) {
        expect(p, TOK_LBRACE, "{");
        node->as.if_stmt.else_branch = parse_block(p);
    }
    return node;
}

// while <expr> { }
static ASTNode *parse_while_stmt(Parser *p) {
    int line = p->current.line;
    advance(p);  // consume 'while'

    ASTNode *node = ast_new_node(NODE_WHILE, line);
    node->as.while_stmt.cond = parse_expr(p);

    expect(p, TOK_LBRACE, "{");
    node->as.while_stmt.body = parse_block(p);
    return node;
}

// func name(a, b, c) { }
static ASTNode *parse_func_decl(Parser *p) {
    int line = p->current.line;
    advance(p);  // consume 'func'

    Token name_tok = expect(p, TOK_IDENT, "function name");
    ASTNode *node  = ast_new_node(NODE_FUNC_DECL, line);
    strncpy(node->as.func_decl.name, name_tok.lexeme, 63);

    expect(p, TOK_LPAREN, "(");

    // parameter list
    int pc = 0;
    if (!check(p, TOK_RPAREN)) {
        do {
            if (pc >= 8) {
                fprintf(stderr, "line %d: max 8 parameters\n", line);
                p->had_error = 1;
                break;
            }
            Token param = expect(p, TOK_IDENT, "parameter name");
            strncpy(node->as.func_decl.params[pc++], param.lexeme, 63);
        } while (match(p, TOK_COMMA));
    }
    node->as.func_decl.param_count = pc;

    expect(p, TOK_RPAREN, ")");
    expect(p, TOK_LBRACE, "{");
    node->as.func_decl.body = parse_block(p);
    return node;
}

// return [<expr>]
static ASTNode *parse_return_stmt(Parser *p) {
    int line = p->current.line;
    advance(p);  // consume 'return'

    ASTNode *node = ast_new_node(NODE_RETURN, line);

    // bare `return` with no value
    if (check(p, TOK_NEWLINE) || check(p, TOK_RBRACE) || check(p, TOK_EOF)) {
        node->as.ret.value = NULL;
    } else {
        node->as.ret.value = parse_expr(p);
    }

    expect_newline(p);
    return node;
}

// print(arg1, arg2, ...)
static ASTNode *parse_print_stmt(Parser *p) {
    int line = p->current.line;
    advance(p);  // consume 'print'

    expect(p, TOK_LPAREN, "(");

    ASTNode *node = ast_new_node(NODE_PRINT, line);
    ASTNode *head = NULL, *tail = NULL;
    int      argc = 0;

    if (!check(p, TOK_RPAREN)) {
        do {
            skip_newlines(p);
            ASTNode *arg = parse_expr(p);
            argc++;
            if (!head) head = tail = arg;
            else       { tail->next = arg; tail = arg; }
        } while (match(p, TOK_COMMA));
    }

    expect(p, TOK_RPAREN, ")");
    node->as.print.args      = head;
    node->as.print.arg_count = argc;
    expect_newline(p);
    return node;
}

// a bare expression on its own line (e.g. a function call for side effects)
static ASTNode *parse_expr_stmt(Parser *p) {
    int line = p->current.line;
    ASTNode *node = ast_new_node(NODE_EXPR_STMT, line);
    node->as.expr_stmt.expr = parse_expr(p);
    expect_newline(p);
    return node;
}

// Statement dispatcher

static ASTNode *parse_stmt(Parser *p) {
    skip_newlines(p);

    if (check(p, TOK_IF))     return parse_if_stmt(p);
    if (check(p, TOK_WHILE))  return parse_while_stmt(p);
    if (check(p, TOK_FUNC))   return parse_func_decl(p);
    if (check(p, TOK_RETURN)) return parse_return_stmt(p);
    if (check(p, TOK_PRINT))  return parse_print_stmt(p);

    // assignment vs expression statement — one token of lookahead
    // current = IDENT, lexer_peek = the token after it
    if (check(p, TOK_IDENT) && lexer_peek(p->lexer).type == TOK_ASSIGN)
        return parse_assign_stmt(p);

    return parse_expr_stmt(p);
}


// expression parsers (grammar hierarchy, lowest → highest precedence)

// Each level calls the level below it first.
// Binary nodes are built bottom-up as we return up the call stack.

static ASTNode *parse_expr(Parser *p) {
    return parse_or_expr(p);
}

// or (lowest precedence among expressions)
static ASTNode *parse_or_expr(Parser *p) {
    ASTNode *left = parse_and_expr(p);

    while (check(p, TOK_OR)) {
        int line = p->current.line;
        advance(p);
        ASTNode *right = parse_and_expr(p);
        ASTNode *node  = ast_new_node(NODE_BINARY, line);
        node->as.binary.op    = TOK_OR;
        node->as.binary.left  = left;
        node->as.binary.right = right;
        left = node;  // left-associate: (a or b) or c
    }
    return left;
}

// and
static ASTNode *parse_and_expr(Parser *p) {
    ASTNode *left = parse_not_expr(p);

    while (check(p, TOK_AND)) {
        int line = p->current.line;
        advance(p);
        ASTNode *right = parse_not_expr(p);
        ASTNode *node  = ast_new_node(NODE_BINARY, line);
        node->as.binary.op    = TOK_AND;
        node->as.binary.left  = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

// not (unary, right-associative: `not not x` is fine)
static ASTNode *parse_not_expr(Parser *p) {
    if (check(p, TOK_NOT)) {
        int line = p->current.line;
        advance(p);
        ASTNode *node = ast_new_node(NODE_UNARY, line);
        node->as.unary.op      = TOK_NOT;
        node->as.unary.operand = parse_not_expr(p);  // recursive for `not not x`
        return node;
    }
    return parse_cmp_expr(p);
}

// == != < > <= >=
static ASTNode *parse_cmp_expr(Parser *p) {
    ASTNode *left = parse_add_expr(p);

    // only ONE comparison per expression — no chaining (a < b < c is an error)
    TokenType op = p->current.type;
    if (op == TOK_EQ  || op == TOK_NEQ ||
        op == TOK_LT  || op == TOK_GT  ||
        op == TOK_LTE || op == TOK_GTE) {
        int line = p->current.line;
        advance(p);
        ASTNode *right = parse_add_expr(p);
        ASTNode *node  = ast_new_node(NODE_BINARY, line);
        node->as.binary.op    = op;
        node->as.binary.left  = left;
        node->as.binary.right = right;
        return node;
    }
    return left;
}

// + -  (left-associative, implemented with a loop not recursion)
static ASTNode *parse_add_expr(Parser *p) {
    ASTNode *left = parse_mul_expr(p);

    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        TokenType op   = p->current.type;
        int       line = p->current.line;
        advance(p);
        ASTNode *right = parse_mul_expr(p);
        ASTNode *node  = ast_new_node(NODE_BINARY, line);
        node->as.binary.op    = op;
        node->as.binary.left  = left;
        node->as.binary.right = right;
        left = node;  // fold into left: 1+2+3 → ((1+2)+3)
    }
    return left;
}

// * / %
static ASTNode *parse_mul_expr(Parser *p) {
    ASTNode *left = parse_unary_expr(p);

    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        TokenType op   = p->current.type;
        int       line = p->current.line;
        advance(p);
        ASTNode *right = parse_unary_expr(p);
        ASTNode *node  = ast_new_node(NODE_BINARY, line);
        node->as.binary.op    = op;
        node->as.binary.left  = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

// unary minus: -x
static ASTNode *parse_unary_expr(Parser *p) {
    if (check(p, TOK_MINUS)) {
        int line = p->current.line;
        advance(p);
        ASTNode *node = ast_new_node(NODE_UNARY, line);
        node->as.unary.op      = TOK_MINUS;
        node->as.unary.operand = parse_unary_expr(p);  // handles --x too
        return node;
    }
    return parse_primary(p);
}

// literals, identifiers, function calls, grouped expressions
static ASTNode *parse_primary(Parser *p) {
    int line = p->current.line;

    // number literal
    if (check(p, TOK_NUMBER)) {
        ASTNode *node = ast_new_node(NODE_NUMBER, line);
        node->as.number.value = p->current.num_val;
        advance(p);
        return node;
    }

    // string literal
    if (check(p, TOK_STRING)) {
        ASTNode *node = ast_new_node(NODE_STRING, line);
        strncpy(node->as.string.value, p->current.lexeme, 255);
        advance(p);
        return node;
    }

    // bool literal
    if (check(p, TOK_BOOL)) {
        ASTNode *node = ast_new_node(NODE_BOOL, line);
        node->as.boolean.value = strcmp(p->current.lexeme, "true") == 0 ? 1 : 0;
        advance(p);
        return node;
    }

    // identifier or function call
    if (check(p, TOK_IDENT)) {
        char name[64];
        strncpy(name, p->current.lexeme, 63);
        advance(p);

        // function call: name(args)
        if (match(p, TOK_LPAREN)) {
            ASTNode *node = ast_new_node(NODE_CALL, line);
            strncpy(node->as.call.name, name, 63);

            ASTNode *head = NULL, *tail = NULL;
            int argc = 0;

            if (!check(p, TOK_RPAREN)) {
                do {
                    skip_newlines(p);
                    ASTNode *arg = parse_expr(p);
                    argc++;
                    if (!head) head = tail = arg;
                    else       { tail->next = arg; tail = arg; }
                    skip_newlines(p);
                } while (match(p, TOK_COMMA));
            }

            expect(p, TOK_RPAREN, ")");
            node->as.call.args      = head;
            node->as.call.arg_count = argc;
            return node;
        }

        // plain identifier
        ASTNode *node = ast_new_node(NODE_IDENT, line);
        strncpy(node->as.ident.name, name, 63);
        return node;
    }

    // grouped expression: (expr)
    if (match(p, TOK_LPAREN)) {
        ASTNode *inner = parse_expr(p);
        expect(p, TOK_RPAREN, ")");
        return inner;  // the parens themselves don't become a node
    }

    // nothing matched — error
    fprintf(stderr, "line %d: unexpected token '%s' in expression\n",
            line, p->current.lexeme);
    p->had_error = 1;
    advance(p);  // skip the bad token so we don't loop forever
    return ast_new_node(NODE_NUMBER, line);  // dummy node to keep tree intact
}

// public api

void parser_init(Parser *p, Lexer *l) {
    p->lexer     = l;
    p->had_error = 0;
    // Prime the parser: fill `current` so the first check() call works
    advance(p);
}

ASTNode *parse_program(Parser *p) {
    ASTNode *root = ast_new_node(NODE_PROGRAM, 1);
    ASTNode *head = NULL, *tail = NULL;

    skip_newlines(p);
    while (!check(p, TOK_EOF)) {
        ASTNode *s = parse_stmt(p);
        if (!s) { skip_newlines(p); continue; }
        if (!head) head = tail = s;
        else       { tail->next = s; tail = s; }
        skip_newlines(p);
    }

    root->as.program.stmts = head;
    return root;
}