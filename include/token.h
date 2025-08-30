#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TK_EOF = 0,
    TK_NEWLINE,
    TK_IDENT,
    TK_NUMBER,
    TK_STRING,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_LBRACK,
    TK_RBRACK,
    TK_COMMA,
    TK_COLON,
    TK_SEMI,
    TK_DOT,
    TK_EQ,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_FOR,
    TK_IN,
    TK_DOTDOT,
    TK_DOTDOT_EQ,
    TK_MESH,
    TK_PART,
    TK_CREATE,
    TK_RETURN,
    TK_IMPORT,
    TK_OVERRIDE,
    TK_CONST,
    TK_IF,
    TK_ELSE,
    TK_EQEQ,
    TK_NEQ,
    TK_LT,
    TK_GT,
    TK_LTE,
    TK_GTE,
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *lexeme;
    int len;
    int line;
    int col;
    double number;
} Token;

typedef struct {
    const char *src;
    const char *cur;
    int line;
    int col;
} Lexer;

void lex_init(Lexer *L, const char *src);

Token lex_next(Lexer *L);

#endif