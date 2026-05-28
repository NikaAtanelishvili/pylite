Lexer   lexer;
Parser  parser;

lexer_init(&lexer, source_code);
parser_init(&parser, &lexer);

ASTNode *tree = parse_program(&parser);

if (parser.had_error) {
    fprintf(stderr, "parse failed\n");
    return 1;
}

eval(tree);