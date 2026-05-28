// lexer.h
#ifndef LEXER_H
#define LEXER_H

#include "token.h"   // Lexer depends on Token and TokenType

typedef struct {
    const char *src;   // pointer into source string (lexer does not own this)
    int         pos;   // current cursor position
    int         line;  // current line number, starts at 1
} Lexer;

void  lexer_init(Lexer *l, const char *source);
Token lexer_next(Lexer *l);   // consume and return next token
Token lexer_peek(Lexer *l);   // return next token WITHOUT advancing

#endif