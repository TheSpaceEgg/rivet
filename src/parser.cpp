#include "parser.hpp"

#include <sstream>

Parser::Parser(Lexer& lex, const DiagnosticEngine& diag)
    : lex_(lex), diag_(diag) {
    advance();
}

void Parser::advance() { cur_ = lex_.next(); }

bool Parser::match(TokenKind k) {
    if (cur_.kind == k) { advance(); return true; }
    return false;
}

bool Parser::expect(TokenKind k, const char* msg) {
    if (cur_.kind == k) { advance(); return true; }
    diag_.error(cur_.loc, msg);
    return false;
}

void Parser::skip_newlines() {
    while (match(TokenKind::Newline)) {}
}

std::string Parser::parse_ident_text(const char* msg) {
    if (cur_.kind != TokenKind::Ident) {
        diag_.error(cur_.loc, msg);
        // consume something to avoid getting stuck
        advance();
        return {};
    }
    std::string s(cur_.lexeme);
    advance();
    return s;
}

static std::string strip_quotes(std::string_view s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return std::string(s.substr(1, s.size() - 2));
    }
    return std::string(s);
}

bool Parser::is_reserved_mode_name(const std::string& s) const {
    return s == "boot" || s == "normal" || s == "idle" || s == "fault" || s == "shutdown";
}

std::string Parser::parse_brace_blob() {
    if (!match(TokenKind::LBrace)) return {};

    std::string out = "{";
    int depth = 1;

    while (cur_.kind != TokenKind::Eof && depth > 0) {
        if (cur_.kind == TokenKind::LBrace) { depth++; out += "{"; advance(); continue; }
        if (cur_.kind == TokenKind::RBrace) { depth--; out += "}"; advance(); continue; }

        if (cur_.kind == TokenKind::Newline) { out += "\n"; advance(); continue; }
        if (cur_.kind == TokenKind::Indent || cur_.kind == TokenKind::Dedent) { advance(); continue; }

        if (!cur_.lexeme.empty()) out += std::string(cur_.lexeme) + " ";
        advance();
    }

    if (depth != 0) diag_.error(cur_.loc, "Unterminated '{' config block");
    return out;
}

NodeDecl Parser::parse_node_decl() {
    Token startTok = cur_;
    expect(TokenKind::KwNode, "Expected 'node'");

    NodeDecl n;
    n.loc = startTok.loc;
    n.name = parse_ident_text("Expected node name (identifier)");
    expect(TokenKind::Colon, "Expected ':' after node name");
    n.type_name = parse_ident_text("Expected node type (identifier)");

    if (cur_.kind == TokenKind::LBrace) n.config_text = parse_brace_blob();

    skip_newlines();
    return n;
}

Stmt Parser::parse_stmt() {
    // Only allow function calls: Ident '(' args? ')'
    if (cur_.kind != TokenKind::Ident) {
        diag_.error(cur_.loc, "Expected a statement (function call like foo(...))");
        advance();
        CallStmt dummy;
        dummy.loc = cur_.loc;
        dummy.callee = "<error>";
        return dummy;
    }

    Token nameTok = cur_;
    std::string callee(cur_.lexeme);
    advance();

    if (!match(TokenKind::LParen)) {
        diag_.error(nameTok.loc, "Unknown statement. Did you mean a function call like name(...) ?");
        CallStmt dummy;
        dummy.loc = nameTok.loc;
        dummy.callee = "<error>";
        return dummy;
    }

    CallStmt call;
    call.loc = nameTok.loc;
    call.callee = callee;

    while (cur_.kind != TokenKind::Eof && cur_.kind != TokenKind::RParen) {
        if (cur_.kind == TokenKind::Ident || cur_.kind == TokenKind::Int ||
            cur_.kind == TokenKind::Float || cur_.kind == TokenKind::String) {
            call.args.emplace_back(std::string(cur_.lexeme));
            advance();
            (void)match(TokenKind::Comma);
            continue;
        }

        diag_.error(cur_.loc, "Expected argument (identifier/number/string) or ')'");
        advance();
    }

    expect(TokenKind::RParen, "Expected ')' to close call");
    return call;
}

std::vector<Stmt> Parser::parse_indented_block_stmts() {
    std::vector<Stmt> stmts;

    expect(TokenKind::Newline, "Expected newline before indented block");
    expect(TokenKind::Indent, "Expected indented block");

    while (cur_.kind != TokenKind::Eof) {
        if (match(TokenKind::Dedent)) break;
        if (match(TokenKind::Newline)) continue;

        // Robustness: if lexer fails to emit DEDENT, bail when a new top-level decl starts at col 1.
        if ((cur_.kind == TokenKind::KwMode || cur_.kind == TokenKind::KwNode) &&
            cur_.loc.col == 1) {
            break;
        }

        stmts.push_back(parse_stmt());
        while (match(TokenKind::Newline)) {}
    }

    return stmts;
}

ModeDecl Parser::parse_mode_decl() {
    Token startTok = cur_;
    expect(TokenKind::KwMode, "Expected 'mode'");

    ModeDecl m;
    m.loc = startTok.loc;

    m.node_name = parse_ident_text("Expected node name after 'mode'");
    expect(TokenKind::Arrow, "Expected '->' after node name");

    // Accept IDENT (system/reserved) OR STRING (node-local)
    m.mode_name.loc = cur_.loc;

    if (cur_.kind == TokenKind::Ident) {
        m.mode_name.is_local_string = false;
        m.mode_name.text = std::string(cur_.lexeme);
        advance();

        // For now, IDENT modes must be reserved (until we add global mode declarations)
        if (!is_reserved_mode_name(m.mode_name.text)) {
            diag_.error(m.mode_name.loc,
                        "Unknown mode name. For now, identifier modes must be one of: boot, normal, idle, fault, shutdown. Use a quoted node-local mode like \"name\" instead.");
        }

    } else if (cur_.kind == TokenKind::String) {
        m.mode_name.is_local_string = true;
        m.mode_name.text = strip_quotes(cur_.lexeme);
        advance();

    } else {
        diag_.error(cur_.loc, "Expected mode name after '->' (identifier or string)");
        advance();
        m.mode_name.is_local_string = false;
        m.mode_name.text = "<error>";
    }

    if (match(TokenKind::KwDo)) {
        // Delegation target remains IDENT only for now
        m.delegate_to = parse_ident_text("Expected target mode name after 'do'");
        if (m.delegate_to && !is_reserved_mode_name(*m.delegate_to)) {
            diag_.error(cur_.loc,
                        "Delegate target must be one of: boot, normal, idle, fault, shutdown (for now)");
        }
        skip_newlines();
        return m;
    }

    m.body = parse_indented_block_stmts();
    skip_newlines();
    return m;
}

Program Parser::parse_program() {
    Program p;
    skip_newlines();

    while (cur_.kind != TokenKind::Eof) {
        if (cur_.kind == TokenKind::KwNode) {
            p.decls.emplace_back(parse_node_decl());
            continue;
        }
        if (cur_.kind == TokenKind::KwMode) {
            p.decls.emplace_back(parse_mode_decl());
            continue;
        }

        diag_.error(cur_.loc, "Expected top-level declaration: 'node' or 'mode'");
        advance();
    }

    return p;
}
