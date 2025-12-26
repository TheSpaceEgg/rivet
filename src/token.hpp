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

    // Keywords
    KwNode, KwMode, KwDo, KwSystemMode,
    KwRequest, KwOnRequest, KwSilent, KwReturn,
    KwFunc, KwPublish, KwOnListen, KwTopic,
    
    // Types
    KwTypeInt, KwTypeFloat, KwTypeString, KwTypeBool,

    // Punctuation
    Colon, Comma, Dot, Arrow, Assign,
    LParen, RParen, LBrace, RBrace,
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string_view lexeme{};
    SourceLoc loc{};
};