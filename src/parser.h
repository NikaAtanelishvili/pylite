// parser.h
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer  *lexer;
    Token   current;    // the token we are looking at right now
    Token   previous;   // the token we just consumed
    int     had_error;  // set to 1 on any parse error, checked by caller
} Parser;

// Initialize parser and prime it with the first token
void     parser_init(Parser *p, Lexer *l);

// Parse the full source and return the root NODE_PROGRAM node
ASTNode *parse_program(Parser *p);

#endif