// token.h
#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    // Literals
    TOK_NUMBER, TOK_STRING, TOK_BOOL,
    // Keywords
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FUNC,
    TOK_RETURN, TOK_PRINT,
    TOK_AND, TOK_OR, TOK_NOT,
    // Identifier
    TOK_IDENT,
    // Arithmetic
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    // Comparison
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    // Delimiters
    TOK_ASSIGN,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA,
    // Control
    TOK_NEWLINE,
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char      lexeme[256];  // raw source text of this token
    double    num_val;      // only used when type == TOK_NUMBER
    int       line;         // line number, for error messages
} Token;

#endif