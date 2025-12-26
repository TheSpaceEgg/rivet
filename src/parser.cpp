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
                cur_.kind == TokenKind::KwTrue || cur_.kind == TokenKind::KwFalse) { 
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

std::vector<std::string> Parser::parse_print_args() {
    std::vector<std::string> args;
    
    // ENFORCE: Only one argument allowed, and it MUST be a string
    if (cur_.kind == TokenKind::String) {
        // We keep the quotes so the codegen regex can find the {braces}
        args.push_back(std::string(cur_.lexeme));
        advance();
    } else {
        diag_.error(cur_.loc, "Format Error: print/log now requires a single interpolated string (e.g., \"val: {x}\")");
        advance();
    }

    // ERROR TRIGGER: If there is a comma, it's now an illegal legacy syntax
    if (cur_.kind == TokenKind::Comma) {
        diag_.error(cur_.loc, "Legacy Syntax: Commas are no longer supported. Use \"{variable}\" interpolation instead.");
        while (cur_.kind != TokenKind::Newline && cur_.kind != TokenKind::Eof) advance();
    }

    return args;
}

int Parser::bin_prec(TokenKind k) const {
    switch (k) {
        case TokenKind::KwOr: return 1;
        case TokenKind::KwAnd: return 2;
        case TokenKind::EqEq:
        case TokenKind::NotEq: return 3;
        case TokenKind::Less:
        case TokenKind::LessEq:
        case TokenKind::Greater:
        case TokenKind::GreaterEq: return 4;
        case TokenKind::Plus:
        case TokenKind::Minus: return 5;
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent: return 6;
        default: return -1;
    }
}

std::optional<BinaryOp> Parser::tok_to_binop(TokenKind k) const {
    switch (k) {
        case TokenKind::KwOr: return BinaryOp::Or;
        case TokenKind::KwAnd: return BinaryOp::And;
        case TokenKind::EqEq: return BinaryOp::Eq;
        case TokenKind::NotEq: return BinaryOp::Neq;
        case TokenKind::Less: return BinaryOp::Lt;
        case TokenKind::LessEq: return BinaryOp::Lte;
        case TokenKind::Greater: return BinaryOp::Gt;
        case TokenKind::GreaterEq: return BinaryOp::Gte;
        case TokenKind::Plus: return BinaryOp::Add;
        case TokenKind::Minus: return BinaryOp::Sub;
        case TokenKind::Star: return BinaryOp::Mul;
        case TokenKind::Slash: return BinaryOp::Div;
        case TokenKind::Percent: return BinaryOp::Mod;
        default: return std::nullopt;
    }
}

ExprPtr Parser::parse_primary() {
    SourceLoc loc = cur_.loc;

    if (cur_.kind == TokenKind::Int) {
        Expr::Literal lit{Expr::Literal::Kind::Int, std::string(cur_.lexeme)};
        advance();
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(lit);
        return e;
    }
    if (cur_.kind == TokenKind::Float) {
        Expr::Literal lit{Expr::Literal::Kind::Float, std::string(cur_.lexeme)};
        advance();
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(lit);
        return e;
    }
    if (cur_.kind == TokenKind::String) {
        Expr::Literal lit{Expr::Literal::Kind::String, std::string(cur_.lexeme)};
        advance();
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(lit);
        return e;
    }
    if (cur_.kind == TokenKind::KwTrue || cur_.kind == TokenKind::KwFalse) {
        Expr::Literal lit{Expr::Literal::Kind::Bool, std::string(cur_.lexeme)};
        advance();
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(lit);
        return e;
    }
    if (cur_.kind == TokenKind::Ident) {
        Expr::Ident id{std::string(cur_.lexeme)};
        advance();
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(id);
        return e;
    }
    if (match(TokenKind::LParen)) {
        auto e = parse_expr(0);
        expect(TokenKind::RParen, "Expected ')'");
        return e;
    }

    diag_.error(cur_.loc, "Expected expression");
    advance();
    auto e = std::make_shared<Expr>();
    e->loc = loc;
    e->v = Expr::Literal{Expr::Literal::Kind::Int, "0"};
    return e;
}

ExprPtr Parser::parse_unary() {
    SourceLoc loc = cur_.loc;
    if (match(TokenKind::KwNot)) {
        Expr::Unary u{UnaryOp::Not, parse_unary()};
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(u);
        return e;
    }
    if (match(TokenKind::Minus)) {
        Expr::Unary u{UnaryOp::Neg, parse_unary()};
        auto e = std::make_shared<Expr>();
        e->loc = loc; e->v = std::move(u);
        return e;
    }
    return parse_primary();
}

ExprPtr Parser::parse_expr(int min_prec) {
    auto lhs = parse_unary();
    while (true) {
        auto op = tok_to_binop(cur_.kind);
        if (!op) break;
        int prec = bin_prec(cur_.kind);
        if (prec < min_prec) break;

        SourceLoc loc = cur_.loc;
        TokenKind opTok = cur_.kind;
        advance();

        auto rhs = parse_expr(prec + 1);

        Expr::Binary b{*op, lhs, rhs};
        auto e = std::make_shared<Expr>();
        e->loc = loc;
        e->v = std::move(b);
        lhs = e;
    }
    return lhs;
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

std::vector<StmtPtr> Parser::parse_indented_block_stmts() {
    std::vector<StmtPtr> stmts;
    expect(TokenKind::Newline, "Expected newline before indented block");
    while (match(TokenKind::Newline)) {}
    expect(TokenKind::Indent, "Expected indented block");

    while (cur_.kind != TokenKind::Eof) {
        if (match(TokenKind::Dedent)) break;
        if (match(TokenKind::Newline)) continue;

        if (auto s = parse_stmt()) {
            stmts.push_back(*s);
        } else {
            advance(); // Infinite loop protection
        }
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
        expect(TokenKind::LParen, "Expected '()'");
        expect(TokenKind::RParen, "Expected ')'");
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
    
    std::string first = parse_ident_text("Expected topic handle");
    if (match(TokenKind::Dot)) {
        decl.source_node = first;
        decl.topic_name = parse_ident_text("Expected topic name");
    } else {
        decl.topic_name = first;
    }

    if (match(TokenKind::KwDo)) {
        decl.delegate_to = parse_ident_text("Expected function");
        expect(TokenKind::LParen, "Expected '()'");
        expect(TokenKind::RParen, "Expected ')'");
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
        }
    }

    if (match(TokenKind::Newline)) {
        while (match(TokenKind::Newline)) {}
        if (match(TokenKind::Indent)) {
            while (cur_.kind != TokenKind::Eof && cur_.kind != TokenKind::Dedent) {
                if (cur_.kind == TokenKind::KwOnRequest) n.requests.push_back(parse_on_request_decl());
                else if (cur_.kind == TokenKind::KwOnListen) n.listeners.push_back(parse_on_listen_decl());
                else if (cur_.kind == TokenKind::KwFunc) n.private_funcs.push_back(parse_func_decl());
                else if (cur_.kind == TokenKind::KwTopic) n.topics.push_back(parse_topic_decl());
                else if (match(TokenKind::Newline)) continue;
                else advance();
            }
            expect(TokenKind::Dedent, "Expected Dedent");
        }
    }
    skip_newlines();
    return n;
}


template <typename T>
static StmtPtr wrap_stmt(T&& node) {
    auto s = std::make_shared<Stmt>();
    s->loc = node.loc;
    s->v = std::forward<T>(node);
    return s;
}

StmtPtr Parser::parse_if_stmt() {
    Token startTok = cur_;
    expect(TokenKind::KwIf, "Expected 'if'");
    IfStmt ifs;
    ifs.loc = startTok.loc;
    ifs.cond = parse_expr(0);
    expect(TokenKind::Colon, "Expected ':' after if condition");
    ifs.then_body = parse_indented_block_stmts();

    // zero or more elif
    while (cur_.kind == TokenKind::KwElif) {
        Token eTok = cur_;
        advance();
        IfElifBranch br;
        br.loc = eTok.loc;
        br.cond = parse_expr(0);
        expect(TokenKind::Colon, "Expected ':' after elif condition");
        br.body = parse_indented_block_stmts();
        ifs.elifs.push_back(std::move(br));
    }

    // optional else
    if (cur_.kind == TokenKind::KwElse) {
        advance();
        expect(TokenKind::Colon, "Expected ':' after else");
        ifs.else_body = parse_indented_block_stmts();
    }

    return wrap_stmt(std::move(ifs));
}

std::optional<StmtPtr> Parser::parse_stmt() {
    if (cur_.kind == TokenKind::KwIf) {
        return parse_if_stmt();
    }

    if (cur_.kind == TokenKind::KwPrint) {
        LogStmt log;
        log.loc = cur_.loc;
        log.level = LogLevel::Print;
        advance();
        log.args = parse_print_args();
        return wrap_stmt(std::move(log));
    }
    if (cur_.kind == TokenKind::KwLog) {
        LogStmt log;
        log.loc = cur_.loc;
        advance(); // consume 'log'

        if (match(TokenKind::KwError))      log.level = LogLevel::Error;
        else if (match(TokenKind::KwWarn))  log.level = LogLevel::Warn;
        else if (match(TokenKind::KwInfo))  log.level = LogLevel::Info;
        else if (match(TokenKind::KwDebug)) log.level = LogLevel::Debug;
        else log.level = LogLevel::Info;

        log.args = parse_print_args();
        return wrap_stmt(std::move(log));
    }
    if (cur_.kind == TokenKind::KwRequest) {
        RequestStmt req;
        req.loc = cur_.loc;
        advance();
        if (match(TokenKind::KwSilent)) req.is_silent = true;
        req.target_node = parse_ident_text("Expected node");
        expect(TokenKind::Dot, "Expected '.'");
        req.func_name = parse_ident_text("Expected func");
        req.args = parse_call_args();
        return wrap_stmt(std::move(req));
    }
    if (cur_.kind == TokenKind::KwReturn) {
        ReturnStmt ret;
        ret.loc = cur_.loc;
        advance();
        if (cur_.kind == TokenKind::Ident || cur_.kind == TokenKind::Int || cur_.kind == TokenKind::String ||
            cur_.kind == TokenKind::KwTrue || cur_.kind == TokenKind::KwFalse) {
            ret.value = std::string(cur_.lexeme);
            advance();
        }
        return wrap_stmt(std::move(ret));
    }
    if (cur_.kind == TokenKind::KwTransition) {
        TransitionStmt t;
        t.loc = cur_.loc;
        advance();
        if (match(TokenKind::KwSystem)) t.is_system = true;
        if (cur_.kind == TokenKind::String) { t.target_state = strip_quotes(cur_.lexeme); advance(); }
        else { t.target_state = parse_ident_text("Expected state"); }
        return wrap_stmt(std::move(t));
    }
    if (cur_.kind == TokenKind::Ident) {
        Token nameTok = cur_;
        std::string identName(cur_.lexeme);
        advance();
        if (cur_.kind == TokenKind::Dot) {
            advance();
            if (match(TokenKind::KwPublish)) {
                PublishStmt pub;
                pub.loc = nameTok.loc;
                pub.topic_handle = identName;
                auto args = parse_call_args();
                if (!args.empty()) pub.value = args[0];
                return wrap_stmt(std::move(pub));
            }
        }
        if (cur_.kind == TokenKind::LParen) {
            CallStmt call;
            call.loc = nameTok.loc;
            call.callee = identName;
            call.args = parse_call_args();
            return wrap_stmt(std::move(call));
        }
    }
    return std::nullopt;
}

ModeDecl Parser::parse_mode_decl() {
    Token startTok = cur_;
    expect(TokenKind::KwMode, "Expected 'mode'");
    ModeDecl m;
    m.loc = startTok.loc;
    m.node_name = parse_ident_text("Expected node");
    expect(TokenKind::Arrow, "Expected '->'");
    m.mode_name = parse_mode_name("Expected mode name");

    if (match(TokenKind::KwIgnore)) {
         if (cur_.kind == TokenKind::KwSystem || cur_.kind == TokenKind::KwController) {
            advance();
            m.ignores_system = true;
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
            else advance();
        }
        expect(TokenKind::Dedent, "Expected Dedent");
    }
    return m;
}

Program Parser::parse_program() {
    Program p;
    skip_newlines();
    while (cur_.kind != TokenKind::Eof) {
        if (cur_.kind == TokenKind::KwSystemMode) p.decls.emplace_back(parse_systemmode_decl());
        else if (cur_.kind == TokenKind::KwNode) p.decls.emplace_back(parse_node_decl());
        else if (cur_.kind == TokenKind::KwMode) p.decls.emplace_back(parse_mode_decl());
        else if (cur_.kind == TokenKind::KwFunc) p.decls.emplace_back(parse_func_decl());
        else if (match(TokenKind::Newline)) continue;
        else advance(); // Infinite loop protection
    }
    return p;
}
