#pragma once
#include "source.hpp"
#include <string_view>

enum class TokenKind {
    // Special
    Eof,
    Invalid,

    // Layout
    Newline,
    Indent,
    Dedent,

    // Identifiers / literals
    Ident,
    Int,
    Float,
    String,

    // Keywords (add as needed)
    KwNode,
    KwMessage,
    KwVar,
    KwFunc,
    KwMode,
    KwOnRequest,
    KwReturn,
    KwDo,
    KwSystemMode,

    // Punctuation / operators
    Colon,      // :
    Comma,      // ,
    Dot,        // .
    Arrow,      // ->
    Assign,     // =
    LParen,     // (
    RParen,     // )
    LBrace,     // {
    RBrace,     // }
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string_view lexeme{};
    SourceLoc loc{};
};
