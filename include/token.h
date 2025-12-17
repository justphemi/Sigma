#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum TokenType {
    // Literals
    TOK_NUMBER, TOK_STRING, TOK_TRUE, TOK_FALSE,
    
    // Keywords
    TOK_FN, TOK_RETURN, TOK_IF, TOK_EL, TOK_FOR, TOK_WHILE,
    TOK_YAP, TOK_TIME_START, TOK_TIME_END,
    
    // Operators
    TOK_COLON, TOK_DCOLON, TOK_ARROW, TOK_PLUS, TOK_MINUS,
    TOK_STAR, TOK_SLASH, TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT,
    TOK_LTE, TOK_GTE, TOK_PLUSPLUS, TOK_MINUSMINUS,
    
    // Delimiters
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACK, TOK_RBRACK, TOK_COMMA, TOK_DOT,
    
    // Other
    TOK_IDENT, TOK_EOF, TOK_UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;
};

#endif