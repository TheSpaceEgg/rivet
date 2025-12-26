#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include "diag.hpp"
#include <vector>
#include <optional>

class Parser {
public:
    Parser(Lexer& lex, const DiagnosticEngine& diag);
    Program parse_program();

private:
    void advance();
    bool match(TokenKind k);
    bool expect(TokenKind k, const char* msg);
    void skip_newlines();
    
    std::string parse_ident_text(const char* msg);
    std::string parse_string_literal(const char* msg);
    std::string parse_brace_blob();
    
    ModeName parse_mode_name(const char* msg);
    TypeInfo parse_type();
    TypeInfo parse_optional_return_type();
    
    std::vector<Param> parse_decl_params();
    std::vector<std::string> parse_call_args();
    
    // NEW: Declaration for the missing function
    std::vector<std::string> parse_print_args();

    std::vector<Stmt> parse_indented_block_stmts();
    
    // Declaration Parsers
    SystemModeDecl parse_systemmode_decl();
    TopicDecl parse_topic_decl();
    FuncDecl parse_func_decl();
    OnRequestDecl parse_on_request_decl();
    OnListenDecl parse_on_listen_decl();
    NodeDecl parse_node_decl();
    ModeDecl parse_mode_decl();
    
    // Statement Parser
    std::optional<Stmt> parse_stmt();

    Lexer& lex_;
    const DiagnosticEngine& diag_;
    Token cur_;
};