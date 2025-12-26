#pragma once
#include "ast.hpp"
#include "diag.hpp"
#include "lexer.hpp"
#include "token.hpp"
#include <optional>
#include <string>
#include <vector>

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
    std::string parse_string_literal(const char* msg); 

    ModeName parse_mode_name(const char* msg);

    std::vector<std::string> parse_call_args();
    std::vector<Param> parse_decl_params();
    TypeInfo parse_type();
    TypeInfo parse_optional_return_type();

    SystemModeDecl parse_systemmode_decl();
    NodeDecl parse_node_decl();
    ModeDecl parse_mode_decl();
    FuncDecl parse_func_decl();
    
    // NEW
    TopicDecl parse_topic_decl();
    
    OnRequestDecl parse_on_request_decl();
    OnListenDecl parse_on_listen_decl();

    std::optional<Stmt> parse_stmt();
    std::vector<Stmt> parse_indented_block_stmts();

    void skip_indented_block_recovery();
};