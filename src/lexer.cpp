#include "lexer.hpp"
#include <cctype>
#include <unordered_map>

static TokenKind keyword_kind(std::string_view s) {
    static const std::unordered_map<std::string_view, TokenKind> kw = {
        {"node", TokenKind::KwNode},
        {"mode", TokenKind::KwMode},
        {"do",   TokenKind::KwDo},
    };
    auto it = kw.find(s);
    return it == kw.end() ? TokenKind::Ident : it->second;
}

Lexer::Lexer(const Source& src, const DiagnosticEngine& diag)
    : src_(src), diag_(diag), text_(src.text()) {}

char Lexer::cur() const { return eof() ? '\0' : text_[i_]; }

char Lexer::peek(int a) const {
    int j = i_ + a;
    return (j < 0 || j >= (int)text_.size()) ? '\0' : text_[j];
}

bool Lexer::eof() const { return i_ >= (int)text_.size(); }

Token Lexer::make(TokenKind k, int s, int e) {
    return {k, text_.substr(s, e - s), src_.loc_from_offset(s)};
}

void Lexer::emit_newline() {
    int s = i_;
    if (cur() == '\r') i_++;
    if (cur() == '\n') i_++;
    pending_.push_back(make(TokenKind::Newline, s, i_));
    at_line_start_ = true;
}

void Lexer::handle_indent() {
    int s = i_;
    int spaces = 0;
    while (cur() == ' ') { spaces++; i_++; }

    int prev = indent_.back();
    if (spaces > prev) {
        indent_.push_back(spaces);
        pending_.push_back(make(TokenKind::Indent, s, s));
    } else {
        while (spaces < indent_.back()) {
            indent_.pop_back();
            pending_.push_back(make(TokenKind::Dedent, s, s));
        }
    }
    at_line_start_ = false;
}

void Lexer::skip_ws() {
    while (cur() == ' ' || cur() == '\t') i_++;
}

void Lexer::skip_line_comment() {
    while (!eof() && cur() != '\n' && cur() != '\r') i_++;
}

void Lexer::skip_block_comment() {
    // Consume /* ... */
    i_ += 2; // consume /*
    while (!eof()) {
        if (cur() == '*' && peek(1) == '/') {
            i_ += 2;
            return;
        }
        if (cur() == '\n' || cur() == '\r') {
            emit_newline();   // keep line numbers correct
        } else {
            i_++;
        }
    }
    diag_.error(src_.loc_from_offset(i_), "Unterminated block comment");
}

Token Lexer::ident() {
    int s = i_;
    while (std::isalnum(cur()) || cur() == '_') i_++;
    auto t = make(TokenKind::Ident, s, i_);
    t.kind = keyword_kind(t.lexeme);
    return t;
}

Token Lexer::number() {
    int s = i_;
    while (std::isdigit(cur())) i_++;
    return make(TokenKind::Int, s, i_);
}

Token Lexer::string() {
    int s = i_++;
    while (!eof() && cur() != '"') {
        if (cur() == '\n' || cur() == '\r') {
            diag_.error(src_.loc_from_offset(i_), "Newline in string literal");
            break;
        }
        i_++;
    }
    if (!eof()) i_++;
    return make(TokenKind::String, s, i_);
}

Token Lexer::next() {
    if (!pending_.empty()) {
        auto t = pending_.front();
        pending_.pop_front();
        return t;
    }

    if (eof()) {
        if (indent_.size() > 1) {
            indent_.pop_back();
            return make(TokenKind::Dedent, i_, i_);
        }
        return make(TokenKind::Eof, i_, i_);
    }

    if (at_line_start_) {
        handle_indent();
        if (!pending_.empty()) return next();
    }

    // Newlines
    if (cur() == '\n' || cur() == '\r') {
        emit_newline();
        return next();
    }

    skip_ws();

    // Comments (NEW)
    if (cur() == '/' && peek(1) == '/') {
        skip_line_comment();
        return next();
    }
    if (cur() == '/' && peek(1) == '*') {
        skip_block_comment();
        return next();
    }

    if (std::isalpha(cur()) || cur() == '_') return ident();
    if (std::isdigit(cur())) return number();
    if (cur() == '"') return string();

    int s = i_;
    if (cur() == ':' ) { i_++; return make(TokenKind::Colon, s, i_); }
    if (cur() == ',' ) { i_++; return make(TokenKind::Comma, s, i_); }
    if (cur() == '(' ) { i_++; return make(TokenKind::LParen, s, i_); }
    if (cur() == ')' ) { i_++; return make(TokenKind::RParen, s, i_); }
    if (cur() == '{' ) { i_++; return make(TokenKind::LBrace, s, i_); }
    if (cur() == '}' ) { i_++; return make(TokenKind::RBrace, s, i_); }
    if (cur() == '-' && peek(1) == '>') { i_ += 2; return make(TokenKind::Arrow, s, i_); }

    diag_.error(src_.loc_from_offset(i_), "Unexpected character");
    i_++;
    return make(TokenKind::Invalid, s, i_);
}
