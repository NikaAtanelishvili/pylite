// main.c
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "eval.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open '%s'\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: pylite <file.pyl>\n"); return 1; }

    char *source = read_file(argv[1]);

    Lexer       lexer;
    Parser      parser;
    Interpreter interp;

    lexer_init (&lexer,  source);
    parser_init(&parser, &lexer);

    ASTNode *tree = parse_program(&parser);
    if (parser.had_error) { fprintf(stderr, "aborting due to parse errors\n"); return 1; }

    interp_init(&interp);
    interp_run (&interp, tree);

    free(source);
    return 0;
}