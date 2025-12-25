#pragma once
#include "ast.hpp"
#include "diag.hpp"
#include "lexer.hpp"
#include "token.hpp"

class Parser {
public:
    Parser(Lexer& lex, const DiagnosticEngine& diag);

    Program parse_program();

private:
    Lexer& lex_;
    const DiagnosticEngine& diag_;
    Token cur_{};

    void advance();
    bool match(TokenKind k);
    bool expect(TokenKind k, const char* msg);
    void skip_newlines();

    std::string parse_ident_text(const char* msg);
    std::string parse_brace_blob();
    
    SystemModeDecl parse_system_mode_decl();
    ModeName parse_mode_name();

    NodeDecl parse_node_decl();
    ModeDecl parse_mode_decl();

    std::vector<Stmt> parse_indented_block_stmts();
    Stmt parse_stmt();

    bool is_reserved_mode_name(const std::string& s) const;
};
