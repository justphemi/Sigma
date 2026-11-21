#pragma once

#include <string>
#include <vector>

#include "common.h"

enum class Tok {
    Eof, Num, Str, Ident,
    // keywords
    Fr, Nah, Grind, In, Dip, Skip, Cook, Serve, Lock, Yolo, Oops, Yeet, Yoink,
    True, False, Nil, And, Or, Not,
    // punctuation
    LParen, RParen, LBrace, RBrace, LBrack, RBrack, Comma, Dot, Colon, Semi, Arrow,
    DotDot, DotDotEq,
    Plus, Minus, Star, Slash, Percent, StarStar,
    Eq, Neq, Lt, Gt, Le, Ge, Bang, AndAnd, OrOr,
    PlusEq, MinusEq, StarEq, SlashEq, PercentEq, PlusPlus,
};

// A piece of a string literal: plain text, or the source of a {expression}.
struct StrPart {
    bool isExpr;
    std::string text;
    Loc loc;
};

struct Token {
    Tok t = Tok::Eof;
    std::string text;          // source text (for strings: the decoded value)
    double num = 0;            // for Tok::Num
    std::vector<StrPart> parts;  // for interpolated strings
    Loc loc;
    bool nl = false;           // first token on its line
};

std::vector<Token> lex(int file, const std::string& src, int line = 1, int col = 1);
