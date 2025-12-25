#pragma once
#include "diag.hpp"
#include "source.hpp"
#include "token.hpp"

#include <deque>
#include <string_view>
#include <vector>

class Lexer {
public:
    Lexer(const Source& src, const DiagnosticEngine& diag);
    Token next();

private:
    const Source& src_;
    const DiagnosticEngine& diag_;
    std::string_view text_;
    int i_ = 0;

    bool at_line_start_ = true;
    std::vector<int> indent_{0};
    std::deque<Token> pending_;

    char cur() const;
    char peek(int a) const;
    bool eof() const;

    void emit_newline();
    void handle_indent();
    void skip_ws();

    void skip_line_comment();
    void skip_block_comment();

    Token make(TokenKind k, int s, int e);

    Token ident();
    Token number();
    Token string();
};
