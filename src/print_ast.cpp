#include "print_ast.hpp"
#include "ast.hpp"
#include <iostream>
#include <string>
#include <variant>

static void indent(std::ostream& os, int depth) {
    for (int i = 0; i < depth; ++i) os << "  ";
}

static void print_type(const TypeInfo& t, std::ostream& os) {
    switch (t.base) {
        case ValType::Int:    os << "int"; break;
        case ValType::Float:  os << "float"; break;
        case ValType::String: os << "string"; break;
        case ValType::Bool:   os << "bool"; break;
        case ValType::Custom: os << t.custom_name; break;
    }
}

static void print_params(const std::vector<Param>& params, std::ostream& os) {
    os << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) os << ", ";
        os << params[i].name << ": ";
        print_type(params[i].type, os);
    }
    os << ")";
}

static void print_expr(const ExprPtr& e, std::ostream& os);

static const char* binop_text(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Eq:  return "==";
        case BinaryOp::Neq: return "!=";
        case BinaryOp::Lt:  return "<";
        case BinaryOp::Lte: return "<=";
        case BinaryOp::Gt:  return ">";
        case BinaryOp::Gte: return ">=";
        case BinaryOp::And: return "and";
        case BinaryOp::Or:  return "or";
    }
    return "?";
}

static void print_expr(const ExprPtr& e, std::ostream& os) {
    if (!e) { os << "<null>"; return; }

    if (auto lit = std::get_if<Expr::Literal>(&e->v)) {
        os << lit->text;
        return;
    }
    if (auto id = std::get_if<Expr::Ident>(&e->v)) {
        os << id->name;
        return;
    }
    if (auto call = std::get_if<Expr::Call>(&e->v)) {
        os << call->callee << "(";
        for (size_t i = 0; i < call->args.size(); ++i) {
            if (i) os << ", ";
            print_expr(call->args[i], os);
        }
        os << ")";
        return;
    }
    if (auto un = std::get_if<Expr::Unary>(&e->v)) {
        if (un->op == UnaryOp::Not) os << "not ";
        else os << "-";
        print_expr(un->rhs, os);
        return;
    }
    if (auto bin = std::get_if<Expr::Binary>(&e->v)) {
        os << "(";
        print_expr(bin->lhs, os);
        os << " " << binop_text(bin->op) << " ";
        print_expr(bin->rhs, os);
        os << ")";
        return;
    }
}

static void print_stmt(const StmtPtr& sp, std::ostream& os, int depth);

static void print_stmts(const std::vector<StmtPtr>& stmts, std::ostream& os, int depth) {
    for (const auto& s : stmts) print_stmt(s, os, depth);
}

static void print_stmt(const StmtPtr& sp, std::ostream& os, int depth) {
    if (!sp) return;

    indent(os, depth);

    if (auto ifs = std::get_if<IfStmt>(&sp->v)) {
        os << "if ";
        print_expr(ifs->cond, os);
        os << ":\n";
        print_stmts(ifs->then_body, os, depth + 1);

        for (const auto& br : ifs->elifs) {
            indent(os, depth);
            os << "elif ";
            print_expr(br.cond, os);
            os << ":\n";
            print_stmts(br.body, os, depth + 1);
        }

        if (!ifs->else_body.empty()) {
            indent(os, depth);
            os << "else:\n";
            print_stmts(ifs->else_body, os, depth + 1);
        }
        return;
    }

    if (auto log = std::get_if<LogStmt>(&sp->v)) {
        if (log->level == LogLevel::Print) os << "print ";
        else {
            os << "log ";
            switch (log->level) {
                case LogLevel::Error: os << "error "; break;
                case LogLevel::Warn:  os << "warn "; break;
                case LogLevel::Debug: os << "debug "; break;
                default: break;
            }
        }
        for (size_t i = 0; i < log->args.size(); ++i) {
            if (i > 0) os << ", ";
            os << log->args[i];
        }
        os << "\n";
        return;
    }

    if (auto call = std::get_if<CallStmt>(&sp->v)) {
        os << call->callee << "(";
        for (size_t i = 0; i < call->args.size(); ++i) {
            if (i > 0) os << ", ";
            os << call->args[i];
        }
        os << ")\n";
        return;
    }

    if (auto req = std::get_if<RequestStmt>(&sp->v)) {
        os << "request ";
        if (req->is_silent) os << "silent ";
        os << req->target_node << "." << req->func_name << "(";
        for (size_t i = 0; i < req->args.size(); ++i) {
            if (i > 0) os << ", ";
            os << req->args[i];
        }
        os << ")\n";
        return;
    }

    if (auto pub = std::get_if<PublishStmt>(&sp->v)) {
        os << pub->topic_handle << ".publish(" << pub->value << ")\n";
        return;
    }

    if (auto ret = std::get_if<ReturnStmt>(&sp->v)) {
        os << "return " << ret->value << "\n";
        return;
    }

    if (auto tr = std::get_if<TransitionStmt>(&sp->v)) {
        os << "transition ";
        if (tr->is_system) os << "system ";
        os << "\"" << tr->target_state << "\"\n";
        return;
    }
}

static void print_listener(const OnListenDecl& lis, std::ostream& os, int depth) {
    indent(os, depth);
    os << "onListen ";
    if (!lis.source_node.empty()) os << lis.source_node << ".";
    os << lis.topic_name << " ";

    if (!lis.delegate_to.empty()) {
        os << "do " << lis.delegate_to << "()\n";
    } else {
        os << lis.sig.name;
        print_params(lis.sig.params, os);
        os << "\n";
        print_stmts(lis.body, os, depth + 1);
    }
}

static void print_modename(const ModeName& mn, std::ostream& os) {
    if (mn.is_local_string) os << "\"" << mn.text << "\"";
    else os << mn.text;
}

void print_ast(const Program& p, std::ostream& os) {
    auto visitor = [&](auto&& x) {
        using T = std::decay_t<decltype(x)>;

        if constexpr (std::is_same_v<T, SystemModeDecl>) {
            os << "systemMode " << x.name << "\n";
        } else if constexpr (std::is_same_v<T, NodeDecl>) {
            os << "\nnode ";
            if (x.is_controller) os << "controller ";
            os << x.name << " : " << x.type_name;
            if (x.ignores_system) os << " ignore system";
            if (!x.config_text.empty()) os << " " << x.config_text;
            os << "\n";

            for (const auto& t : x.topics) {
                indent(os, 1);
                os << "topic " << t.name << " = \"" << t.path << "\" : ";
                print_type(t.type, os);
                os << "\n";
            }

            for (const auto& r : x.requests) {
                indent(os, 1);
                os << "onRequest ";
                if (!r.delegate_to.empty()) {
                    os << "do " << r.delegate_to << "()\n";
                } else {
                    os << r.sig.name;
                    print_params(r.sig.params, os);
                    os << " -> ";
                    print_type(r.sig.return_type, os);
                    os << "\n";
                    print_stmts(r.body, os, 2);
                }
            }

            for (const auto& l : x.listeners) print_listener(l, os, 1);

            for (const auto& f : x.private_funcs) {
                indent(os, 1);
                os << "func " << f.sig.name;
                print_params(f.sig.params, os);
                os << " -> ";
                print_type(f.sig.return_type, os);
                os << "\n";
                print_stmts(f.body, os, 2);
            }
        } else if constexpr (std::is_same_v<T, ModeDecl>) {
            os << "\nmode " << x.node_name << "->";
            print_modename(x.mode_name, os);
            if (x.ignores_system) os << " ignore system";
            os << "\n";

            print_stmts(x.body, os, 1);

            for (const auto& l : x.listeners) print_listener(l, os, 1);
        } else if constexpr (std::is_same_v<T, FuncDecl>) {
            os << "func " << x.sig.name << "\n";
        }
    };

    for (const auto& decl : p.decls) std::visit(visitor, decl);
}
