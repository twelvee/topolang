#include "token.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static int isident1(int c) { return isalpha(c) || c == '_'; }

static int isident(int c) { return isalnum(c) || c == '_'; }

void lex_init(Lexer *L, const char *src) {
    L->src = src;
    L->cur = src;
    L->line = 1;
    L->col = 1;
}

static Token make(Lexer *L, TokenKind k, const char *s, int n) {
    Token t;
    t.kind = k;
    t.lexeme = s;
    t.len = n;
    t.line = L->line;
    t.col = L->col;
    t.number = 0;
    return t;
}

static void adv(Lexer *L) {
    if (*L->cur == '\n') {
        L->line++;
        L->col = 1;
    } else L->col++;
    L->cur++;
}

static Token keyword(Lexer *L, const char *s, int n, Token def) {
    if (n == 4 && !strncmp(s, "mesh", 4)) return make(L, TK_MESH, s, n);
    if (n == 4 && !strncmp(s, "part", 4)) return make(L, TK_PART, s, n);
    if (n == 6 && !strncmp(s, "create", 6)) return make(L, TK_CREATE, s, n);
    if (n == 6 && !strncmp(s, "return", 6)) return make(L, TK_RETURN, s, n);
    if (n == 6 && !strncmp(s, "import", 6)) return make(L, TK_IMPORT, s, n);
    if (n == 8 && !strncmp(s, "override", 8)) return make(L, TK_OVERRIDE, s, n);
    if (n == 3 && !strncmp(s, "for", 3)) return make(L, TK_FOR, s, n);
    if (n == 2 && !strncmp(s, "in", 2)) return make(L, TK_IN, s, n);
    if (n == 5 && !strncmp(s, "const", 5)) return make(L, TK_CONST, s, n);
    return def;
}

static Token make_at(int line, int col, TokenKind k, const char *s, int n) {
    Token t;
    t.kind = k;
    t.lexeme = s;
    t.len = n;
    t.line = line;
    t.col = col;
    t.number = 0;
    return t;
}

Token lex_next(Lexer *L) {
    for (;;) {
        char c = *L->cur;
        if (c == 0) return make(L, TK_EOF, L->cur, 0);
        if (c == ' ' || c == '\t' || c == '\r') {
            adv(L);
            continue;
        }
        if (c == '\n') {
            adv(L);
            return make(L, TK_NEWLINE, L->cur - 1, 1);
        }
        if (c == '/' && L->cur[1] == '/') {
            while (*L->cur && *L->cur != '\n') adv(L);
            continue;
        }
        switch (c) {
            case '+':;
                int line = L->line, col = L->col;
                adv(L);
                return make_at(line, col, TK_PLUS, L->cur - 1, 1);
            case '-':
                adv(L);
                return make(L, TK_MINUS, L->cur - 1, 1);
            case '*':
                adv(L);
                return make(L, TK_STAR, L->cur - 1, 1);
            case '/':
                adv(L);
                return make(L, TK_SLASH, L->cur - 1, 1);
            case '(':
                adv(L);
                return make(L, TK_LPAREN, L->cur - 1, 1);
            case ')':
                adv(L);
                return make(L, TK_RPAREN, L->cur - 1, 1);
            case '{':
                adv(L);
                return make(L, TK_LBRACE, L->cur - 1, 1);
            case '}':
                adv(L);
                return make(L, TK_RBRACE, L->cur - 1, 1);
            case '[':
                adv(L);
                return make(L, TK_LBRACK, L->cur - 1, 1);
            case ']':
                adv(L);
                return make(L, TK_RBRACK, L->cur - 1, 1);
            case ',':
                adv(L);
                return make(L, TK_COMMA, L->cur - 1, 1);
            case ':':
                adv(L);
                return make(L, TK_COLON, L->cur - 1, 1);
            case ';':
                adv(L);
                return make(L, TK_SEMI, L->cur - 1, 1);
            case '.': {
                int line = L->line, col = L->col;
                adv(L);
                if (*L->cur == '.') {
                    adv(L);
                    if (*L->cur == '=') {
                        adv(L);
                        return make_at(line, col, TK_DOTDOT_EQ, L->cur - 3, 3);
                    }
                    return make_at(line, col, TK_DOTDOT, L->cur - 2, 2);
                }
                return make_at(line, col, TK_DOT, L->cur - 1, 1);
            }
            case '=':
                adv(L);
                return make(L, TK_EQ, L->cur - 1, 1);
            case '"': {
                const char *s = L->cur + 1;
                int n = 0;
                adv(L);
                while (*L->cur && *L->cur != '"') {
                    adv(L);
                    n++;
                }
                Token t = make(L, TK_STRING, s, n);
                if (*L->cur == '"') adv(L);
                return t;
            }
        }
        if (isdigit((unsigned char) c) || (c == '.' && isdigit((unsigned char) L->cur[1]))) {
            int line = L->line, col = L->col;
            const char *s = L->cur;
            int n = 0, seen_dot = 0;
            while (isdigit((unsigned char) *L->cur) ||
                   (*L->cur == '.' && !seen_dot && isdigit((unsigned char) L->cur[1]))) {
                if (*L->cur == '.') seen_dot = 1;
                adv(L);
                n++;
            }
            Token t = make_at(line, col, TK_NUMBER, s, n);
            t.number = strtod(s, NULL);
            return t;
        }
        if (isident1((unsigned char) c)) {
            const char *s = L->cur;
            int n = 0;
            while (isident((unsigned char) *L->cur)) {
                adv(L);
                n++;
            }
            Token id = make(L, TK_IDENT, s, n);
            return keyword(L, s, n, id);
        }
        adv(L);
    }
}
