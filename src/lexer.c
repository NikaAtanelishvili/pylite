#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "lexer.h"   // includes token.h transitively

// helpers
static char current(Lexer *l)  { return l->src[l->pos]; }

static char advance(Lexer *l)  { return l->src[l->pos++]; }

static void skip_whitespace(Lexer *l) {
    while (current(l) == ' ' || current(l) == '\t' || current(l) == '\r')
        advance(l);
}

static Token make_token(TokenType type, const char *lexeme, int line) {
    Token t;
    t.type    = type;
    t.num_val = 0;
    t.line    = line;
    strncpy(t.lexeme, lexeme, sizeof(t.lexeme) - 1);
    t.lexeme[sizeof(t.lexeme) - 1] = '\0';
    return t;
}

static Token make_number_token(double val, int line) {
    Token t = make_token(TOK_NUMBER, "", line);
    t.num_val = val;
    snprintf(t.lexeme, sizeof(t.lexeme), "%g", val); // 01000000010001010100000000000000000 -> "67.3"
    return t;
}

static Token make_string_token(const char *str, int line) {
    return make_token(TOK_STRING, str, line);
}

static const struct { const char *word; TokenType type; } KEYWORDS[] = {
    {"if",TOK_IF},{"else",TOK_ELSE},{"while",TOK_WHILE},{"func",TOK_FUNC},
    {"return",TOK_RETURN},{"print",TOK_PRINT},
    {"and",TOK_AND},{"or",TOK_OR},{"not",TOK_NOT},
    {"true",TOK_BOOL},{"false",TOK_BOOL},
    {NULL, 0}
};

static Token make_ident_or_keyword(const char *buf, int line) {
    for (int i = 0; KEYWORDS[i].word; i++)
        if (strcmp(buf, KEYWORDS[i].word) == 0)
            return make_token(KEYWORDS[i].type, buf, line);
    return make_token(TOK_IDENT, buf, line);
}

// public api
void lexer_init(Lexer *l, const char *source) {
    l->src  = source;
    l->pos  = 0;
    l->line = 1;
}

Token lexer_peek(Lexer *l) {
    int saved_pos  = l->pos;
    int saved_line = l->line;
    Token t = lexer_next(l);
    l->pos  = saved_pos;
    l->line = saved_line;
    return t;
}

// every time the parser asks for the next token, this fires
Token lexer_next(Lexer *l) {
    skip_whitespace(l);
    char c = current(l);

    if (c == '#') {
        while (current(l) && current(l) != '\n') advance(l);

        /* lexer deletes the comment from its perspective and immediately 
        triggers itself again to look for a real token on the next line */
        return lexer_next(l);
    }

    if (c == '\n') {
        advance(l);
        l->line++;
        return make_token(TOK_NEWLINE, "\\n", l->line);
    }

    if (isdigit(c)) {
        char buf[64]; int len = 0;
        while (isdigit(current(l)))  buf[len++] = advance(l);
        if (current(l) == '.' && isdigit(l->src[l->pos + 1])) {
            buf[len++] = advance(l);
            while (isdigit(current(l))) buf[len++] = advance(l);
        }
        buf[len] = '\0';
        return make_number_token(atof(buf), l->line); // ASCII to Float
    }

    if (c == '"') {
        advance(l);
        char buf[256]; int len = 0;
        while (current(l) && current(l) != '"' && current(l) != '\n')
            buf[len++] = advance(l);
        if (current(l) != '"')
            fprintf(stderr, "line %d: unterminated string\n", l->line);
        else
            advance(l);
        buf[len] = '\0';
        return make_string_token(buf, l->line);
    }

    if (isalpha(c) || c == '_') {
        char buf[64]; int len = 0;
        while (isalnum(current(l)) || current(l) == '_') buf[len++] = advance(l);
        buf[len] = '\0';
        return make_ident_or_keyword(buf, l->line);
    }

    advance(l);
    switch (c) {
        case '=': return current(l)=='=' ? (advance(l), make_token(TOK_EQ,    "==", l->line))
                                         :               make_token(TOK_ASSIGN, "=", l->line);
        case '!': if (current(l)=='=') { advance(l); return make_token(TOK_NEQ, "!=", l->line); }
                  fprintf(stderr, "line %d: unexpected '!'\n", l->line);
                  return lexer_next(l);
        case '<': return current(l)=='=' ? (advance(l), make_token(TOK_LTE, "<=", l->line))
                                         :               make_token(TOK_LT,  "<",  l->line);
        case '>': return current(l)=='=' ? (advance(l), make_token(TOK_GTE, ">=", l->line))
                                         :               make_token(TOK_GT,  ">",  l->line);
        case '+': return make_token(TOK_PLUS,    "+", l->line);
        case '-': return make_token(TOK_MINUS,   "-", l->line);
        case '*': return make_token(TOK_STAR,    "*", l->line);
        case '/': return make_token(TOK_SLASH,   "/", l->line);
        case '%': return make_token(TOK_PERCENT, "%", l->line);
        case '(': return make_token(TOK_LPAREN,  "(", l->line);
        case ')': return make_token(TOK_RPAREN,  ")", l->line);
        case '{': return make_token(TOK_LBRACE,  "{", l->line);
        case '}': return make_token(TOK_RBRACE,  "}", l->line);
        case ',': return make_token(TOK_COMMA,   ",", l->line);
        case '\0': return make_token(TOK_EOF,    "",  l->line);
        default:
            fprintf(stderr, "line %d: unknown character '%c'\n", l->line, c);
            return lexer_next(l);
    }
}