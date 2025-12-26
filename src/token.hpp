#pragma once
#include "source.hpp"
#include <string_view>

enum class TokenKind {
    // Special
    Eof, Invalid,

    // Layout
    Newline, Indent, Dedent,

    // Identifiers / literals
    Ident, Int, Float, String,

    // Keywords (decls / statements)
    KwNode, KwMode, KwDo, KwSystemMode,
    KwRequest, KwOnRequest, KwSilent, KwReturn,
    KwFunc, KwPublish, KwOnListen, KwTopic,
    KwTransition, KwSystem, KwController, KwIgnore,

    // Log & Print
    KwLog, KwPrint,
    KwError, KwWarn, KwInfo, KwDebug,

    // Types
    KwTypeInt, KwTypeFloat, KwTypeString, KwTypeBool,

    // Conditionals / boolean ops
    KwIf, KwElif, KwElse,
    KwAnd, KwOr, KwNot,
    KwTrue, KwFalse,

    // Punctuation / operators
    Colon, Comma, Dot, Arrow, Assign,
    LParen, RParen, LBrace, RBrace,

    Plus, Minus, Star, Slash, Percent,
    EqEq, NotEq,
    Less, LessEq, Greater, GreaterEq,
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string_view lexeme{};
    SourceLoc loc{};
};
