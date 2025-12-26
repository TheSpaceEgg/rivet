#include "parser.hpp"
#include <string>

static std::string strip_quotes(std::string_view s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return std::string(s.substr(1, s.size() - 2));
    return std::string(s);
}

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
        advance();
        return {};
    }
    std::string s(cur_.lexeme);
    advance();
    return s;
}

std::string Parser::parse_string_literal(const char* msg) {
    if (cur_.kind != TokenKind::String) {
        diag_.error(cur_.loc, msg);
        advance();
        return {};
    }
    std::string s = strip_quotes(cur_.lexeme);
    advance();
    return s;
}

ModeName Parser::parse_mode_name(const char* msg) {
    ModeName m;
    m.loc = cur_.loc;
    if (cur_.kind == TokenKind::Ident) {
        m.is_local_string = false;
        m.text = std::string(cur_.lexeme);
        advance();
        return m;
    }
    if (cur_.kind == TokenKind::String) {
        m.is_local_string = true;
        m.text = strip_quotes(cur_.lexeme);
        advance();
        return m;
    }
    diag_.error(cur_.loc, msg);
    advance();
    m.is_local_string = false;
    m.text = "<error>";
    return m;
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

std::vector<std::string> Parser::parse_call_args() {
    std::vector<std::string> args;
    expect(TokenKind::LParen, "Expected '('");
    if (cur_.kind != TokenKind::RParen) {
        while (true) {
            if (cur_.kind == TokenKind::Ident || cur_.kind == TokenKind::Int ||
                cur_.kind == TokenKind::Float || cur_.kind == TokenKind::String ||
                cur_.kind == TokenKind::KwTypeBool) { 
                args.push_back(std::string(cur_.lexeme));
                advance();
            } else {
                diag_.error(cur_.loc, "Expected argument value");
                advance();
            }
            if (!match(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RParen, "Expected ')'");
    return args;
}

// Helper to parse print args without parentheses (space/comma separated)
std::vector<std::string> Parser::parse_print_args() {
    std::vector<std::string> args;
    while (cur_.kind == TokenKind::String || cur_.kind == TokenKind::Ident || 
           cur_.kind == TokenKind::Int || cur_.kind == TokenKind::Float || 
           cur_.kind == TokenKind::KwTypeBool) {
        
        // Handle String specially to keep quotes for AST printer/codegen (or strip them? let's keep them logic consistent)
        // Actually for log arguments we probably want to pass the raw token text so the validator can resolve it later.
        if (cur_.kind == TokenKind::String) {
             args.push_back("\"" + strip_quotes(cur_.lexeme) + "\"");
        } else {
             args.push_back(std::string(cur_.lexeme));
        }
        advance();
        if (!match(TokenKind::Comma)) break; 
    }
    return args;
}

TypeInfo Parser::parse_type() {
    TypeInfo t;
    if (match(TokenKind::KwTypeInt))    { t.base = ValType::Int; return t; }
    if (match(TokenKind::KwTypeFloat))  { t.base = ValType::Float; return t; }
    if (match(TokenKind::KwTypeString)) { t.base = ValType::String; return t; }
    if (match(TokenKind::KwTypeBool))   { t.base = ValType::Bool; return t; }
    if (cur_.kind == TokenKind::Ident) {
        t.base = ValType::Custom;
        t.custom_name = std::string(cur_.lexeme);
        advance();
        return t;
    }
    diag_.error(cur_.loc, "Expected type name");
    advance();
    return t;
}

std::vector<Param> Parser::parse_decl_params() {
    std::vector<Param> params;
    expect(TokenKind::LParen, "Expected '('");
    if (cur_.kind != TokenKind::RParen) {
        while (true) {
            Param p;
            p.loc = cur_.loc;
            p.name = parse_ident_text("Expected parameter name");
            expect(TokenKind::Colon, "Expected ':'");
            p.type = parse_type();
            params.push_back(p);
            if (!match(TokenKind::Comma)) break;
        }
    }
    expect(TokenKind::RParen, "Expected ')'");
    return params;
}

TypeInfo Parser::parse_optional_return_type() {
    if (match(TokenKind::Arrow)) return parse_type();
    TypeInfo t; t.base = ValType::Int; return t;
}

SystemModeDecl Parser::parse_systemmode_decl() {
    Token startTok = cur_;
    expect(TokenKind::KwSystemMode, "Expected 'systemMode'");
    SystemModeDecl d;
    d.loc = startTok.loc;
    d.name = parse_ident_text("Expected system mode name");
    skip_newlines();
    return d;
}

TopicDecl Parser::parse_topic_decl() {
    Token start = cur_;
    expect(TokenKind::KwTopic, "Expected 'topic'");
    TopicDecl t;
    t.loc = start.loc;
    t.name = parse_ident_text("Expected topic handle name");
    expect(TokenKind::Assign, "Expected '='");
    t.path = parse_string_literal("Expected topic path string");
    expect(TokenKind::Colon, "Expected ':'");
    t.type = parse_type();
    skip_newlines();
    return t;
}

std::vector<Stmt> Parser::parse_indented_block_stmts() {
    std::vector<Stmt> stmts;
    expect(TokenKind::Newline, "Expected newline before indented block");
    while (match(TokenKind::Newline)) {} 
    expect(TokenKind::Indent, "Expected indented block");
    
    while (cur_.kind != TokenKind::Eof) {
        if (match(TokenKind::Dedent)) break;
        if (match(TokenKind::Newline)) continue;
        
        if ((cur_.kind == TokenKind::KwMode || cur_.kind == TokenKind::KwNode || 
             cur_.kind == TokenKind::KwSystemMode || cur_.kind == TokenKind::KwFunc || 
             cur_.kind == TokenKind::KwTopic) &&
            cur_.loc.col == 1) break;

        if (auto s = parse_stmt()) stmts.push_back(*s);
        while (match(TokenKind::Newline)) {}
    }
    return stmts;
}

FuncDecl Parser::parse_func_decl() {
    Token start = cur_;
    expect(TokenKind::KwFunc, "Expected 'func'");
    FuncDecl f;
    f.sig.loc = start.loc;
    f.sig.name = parse_ident_text("Expected function name");
    f.sig.params = parse_decl_params();
    f.sig.return_type = parse_optional_return_type();
    f.body = parse_indented_block_stmts();
    skip_newlines();
    return f;
}

OnRequestDecl Parser::parse_on_request_decl() {
    Token start = cur_;
    expect(TokenKind::KwOnRequest, "Expected 'onRequest'");
    OnRequestDecl decl;
    decl.sig.loc = start.loc;

    if (match(TokenKind::KwDo)) {
        decl.delegate_to = parse_ident_text("Expected function name to delegate to");
        decl.sig.name = decl.delegate_to; 
        expect(TokenKind::LParen, "Expected '()' after delegated function name");
        expect(TokenKind::RParen, "Expected ')' after delegated function name");
        skip_newlines();
        return decl;
    }

    decl.sig.name = parse_ident_text("Expected function name");
    decl.sig.params = parse_decl_params();
    decl.sig.return_type = parse_optional_return_type();
    decl.body = parse_indented_block_stmts();
    skip_newlines(); 
    return decl;
}

OnListenDecl Parser::parse_on_listen_decl() {
    Token start = cur_;
    expect(TokenKind::KwOnListen, "Expected 'onListen'");
    OnListenDecl decl;
    decl.loc = start.loc;
    
    std::string first = parse_ident_text("Expected topic handle name");
    if (match(TokenKind::Dot)) {
        decl.source_node = first;
        decl.topic_name = parse_ident_text("Expected topic name after '.'");
    } else {
        decl.topic_name = first;
    }

    if (match(TokenKind::KwDo)) {
        decl.delegate_to = parse_ident_text("Expected function name to delegate to");
        expect(TokenKind::LParen, "Expected '()' after delegated function name");
        expect(TokenKind::RParen, "Expected ')' after delegated function name");
        skip_newlines();
        return decl;
    }

    decl.sig.name = parse_ident_text("Expected handler name");
    decl.sig.params = parse_decl_params();
    decl.body = parse_indented_block_stmts();
    skip_newlines();
    return decl;
}

NodeDecl Parser::parse_node_decl() {
    Token startTok = cur_;
    expect(TokenKind::KwNode, "Expected 'node'");
    NodeDecl n;
    n.loc = startTok.loc;

    if (match(TokenKind::KwController)) n.is_controller = true;

    n.name = parse_ident_text("Expected node name");
    expect(TokenKind::Colon, "Expected ':'");
    n.type_name = parse_ident_text("Expected node type");
    if (cur_.kind == TokenKind::LBrace) n.config_text = parse_brace_blob();

    if (match(TokenKind::KwIgnore)) {
        if (cur_.kind == TokenKind::KwSystem || cur_.kind == TokenKind::KwController) {
            advance();
            n.ignores_system = true;
        } else {
            diag_.error(cur_.loc, "Expected 'system' or 'controller' after 'ignore'");
        }
    }

    if (match(TokenKind::Newline)) {
        while (match(TokenKind::Newline)) {}
        if (match(TokenKind::Indent)) {
            while (cur_.kind != TokenKind::Eof && cur_.kind != TokenKind::Dedent) {
                if (cur_.kind == TokenKind::KwOnRequest) {
                    n.requests.push_back(parse_on_request_decl());
                } else if (cur_.kind == TokenKind::KwOnListen) { 
                    n.listeners.push_back(parse_on_listen_decl());
                } else if (cur_.kind == TokenKind::KwFunc) {
                    n.private_funcs.push_back(parse_func_decl());
                } else if (cur_.kind == TokenKind::KwTopic) { 
                    n.topics.push_back(parse_topic_decl());
                } else if (match(TokenKind::Newline)) {
                    continue;
                } else {
                    diag_.error(cur_.loc, "Expected 'onRequest', 'onListen', 'topic', or 'func' inside node body");
                    while (cur_.kind != TokenKind::Newline && cur_.kind != TokenKind::Eof) advance();
                }
            }
            expect(TokenKind::Dedent, "Expected end of node block");
        }
    }
    skip_newlines();
    return n;
}

std::optional<Stmt> Parser::parse_stmt() {
    // NEW: Log & Print Support
    if (cur_.kind == TokenKind::KwPrint) {
        LogStmt log;
        log.loc = cur_.loc;
        log.level = LogLevel::Print;
        advance();
        log.args = parse_print_args();
        return log;
    }
    if (cur_.kind == TokenKind::KwLog) {
        LogStmt log;
        log.loc = cur_.loc;
        advance();
        
        // Optional level
        if (match(TokenKind::KwError))      log.level = LogLevel::Error;
        else if (match(TokenKind::KwWarn))  log.level = LogLevel::Warn;
        else if (match(TokenKind::KwInfo))  log.level = LogLevel::Info;
        else if (match(TokenKind::KwDebug)) log.level = LogLevel::Debug;
        else log.level = LogLevel::Info; // Default

        log.args = parse_print_args();
        return log;
    }

    if (cur_.kind == TokenKind::KwRequest) {
        RequestStmt req;
        req.loc = cur_.loc;
        advance(); 
        if (match(TokenKind::KwSilent)) req.is_silent = true;
        req.target_node = parse_ident_text("Expected target node name");
        expect(TokenKind::Dot, "Expected '.'");
        req.func_name = parse_ident_text("Expected function name");
        req.args = parse_call_args(); 
        return req;
    }
    if (cur_.kind == TokenKind::KwReturn) {
        ReturnStmt ret;
        ret.loc = cur_.loc;
        advance();
        if (cur_.kind == TokenKind::Ident || cur_.kind == TokenKind::Int ||
            cur_.kind == TokenKind::Float || cur_.kind == TokenKind::String) {
            ret.value = std::string(cur_.lexeme);
            advance();
        }
        return ret;
    }
    
    if (cur_.kind == TokenKind::KwTransition) {
        TransitionStmt t;
        t.loc = cur_.loc;
        advance(); 
        if (match(TokenKind::KwSystem)) {
            t.is_system = true;
        }
        if (cur_.kind == TokenKind::String) {
             t.target_state = strip_quotes(cur_.lexeme);
             advance();
        } else if (cur_.kind == TokenKind::Ident) {
             t.target_state = std::string(cur_.lexeme);
             advance();
        } else {
            diag_.error(cur_.loc, "Expected target state name");
        }
        return t;
    }
    
    if (cur_.kind == TokenKind::Ident) {
        Token nameTok = cur_;
        std::string identName(cur_.lexeme);
        advance();
        
        if (cur_.kind == TokenKind::Dot) {
            advance(); 
            if (cur_.kind == TokenKind::KwPublish) {
                advance(); 
                PublishStmt pub;
                pub.loc = nameTok.loc;
                pub.topic_handle = identName;
                std::vector<std::string> args = parse_call_args();
                if (!args.empty()) pub.value = args[0]; 
                else diag_.error(cur_.loc, "Expected value to publish");
                return pub;
            } else {
                diag_.error(cur_.loc, "Unknown method (only .publish is supported)");
                return std::nullopt;
            }
        }
        
        if (cur_.kind == TokenKind::LParen) {
            CallStmt call;
            call.loc = nameTok.loc;
            call.callee = identName;
            call.args = parse_call_args();
            return call;
        } 
        
        diag_.error(nameTok.loc, "Expected '(' or '.' after identifier");
        return std::nullopt;
    }
    return std::nullopt;
}

ModeDecl Parser::parse_mode_decl() {
    Token startTok = cur_;
    expect(TokenKind::KwMode, "Expected 'mode'");
    ModeDecl m;
    m.loc = startTok.loc;
    m.node_name = parse_ident_text("Expected node name");
    expect(TokenKind::Arrow, "Expected '->'");
    m.mode_name = parse_mode_name("Expected mode name");

    if (match(TokenKind::KwIgnore)) {
         if (cur_.kind == TokenKind::KwSystem || cur_.kind == TokenKind::KwController) {
            advance();
            m.ignores_system = true;
        } else {
            diag_.error(cur_.loc, "Expected 'system' or 'controller' after 'ignore'");
        }
    }

    skip_newlines();

    if (match(TokenKind::Indent)) {
        while (cur_.kind != TokenKind::Eof && cur_.kind != TokenKind::Dedent) {
            if (cur_.kind == TokenKind::KwOnListen) {
                m.listeners.push_back(parse_on_listen_decl());
            } 
            else if (auto s = parse_stmt()) {
                m.body.push_back(*s);
                while (match(TokenKind::Newline)) {}
            }
            else if (match(TokenKind::Newline)) {
                continue;
            } 
            else {
                diag_.error(cur_.loc, "Expected statement or 'onListen' inside mode");
                while (cur_.kind != TokenKind::Newline && cur_.kind != TokenKind::Eof) advance();
            }
        }
        expect(TokenKind::Dedent, "Expected end of mode block");
    }

    return m;
}

Program Parser::parse_program() {
    Program p;
    skip_newlines();
    while (cur_.kind != TokenKind::Eof) {
        if (cur_.kind == TokenKind::KwSystemMode) { p.decls.emplace_back(parse_systemmode_decl()); continue; }
        if (cur_.kind == TokenKind::KwNode)       { p.decls.emplace_back(parse_node_decl()); continue; }
        if (cur_.kind == TokenKind::KwMode)       { p.decls.emplace_back(parse_mode_decl()); continue; }
        if (cur_.kind == TokenKind::KwFunc)       { p.decls.emplace_back(parse_func_decl()); continue; }
        
        if (match(TokenKind::Newline)) continue;

        diag_.error(cur_.loc, "Expected top-level declaration");
        advance();
    }
    return p;
}