#include "print_ast.hpp"
#include "ast.hpp"
#include <iostream>
#include <string>
#include <variant>

// ... (Keep helpers: indent, print_type, print_params, print_stmt, print_stmts)

static void indent(std::ostream& os, int depth) {
    for (int i = 0; i < depth; ++i) os << "  ";
}

static void print_type(const TypeInfo& t, std::ostream& os) {
    switch(t.base) {
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

static void print_stmt(const Stmt& stmt, std::ostream& os, int depth) {
    indent(os, depth);
    if (auto call = std::get_if<CallStmt>(&stmt)) {
        os << call->callee << "(";
        for (size_t i = 0; i < call->args.size(); ++i) {
            if (i > 0) os << ", ";
            os << call->args[i];
        }
        os << ")\n";
    }
    else if (auto req = std::get_if<RequestStmt>(&stmt)) {
        os << "request ";
        if (req->is_silent) os << "silent ";
        os << req->target_node << "." << req->func_name << "(";
        for (size_t i = 0; i < req->args.size(); ++i) {
            if (i > 0) os << ", ";
            os << req->args[i];
        }
        os << ")\n";
    }
    else if (auto pub = std::get_if<PublishStmt>(&stmt)) {
        os << pub->topic_handle << ".publish(" << pub->value << ")\n";
    }
    else if (auto ret = std::get_if<ReturnStmt>(&stmt)) {
        os << "return " << ret->value << "\n";
    }
    else if (auto tr = std::get_if<TransitionStmt>(&stmt)) {
        os << "transition ";
        if (tr->is_system) os << "system ";
        os << "\"" << tr->target_state << "\"\n";
    }
}

static void print_stmts(const std::vector<Stmt>& stmts, std::ostream& os, int depth) {
    for (const auto& s : stmts) print_stmt(s, os, depth);
}

// ---------------------------------------------------------
// Component Printers
// ---------------------------------------------------------

static void print_listener(const OnListenDecl& lis, std::ostream& os, int depth) {
    indent(os, depth);
    os << "onListen ";
    if (!lis.source_node.empty()) os << lis.source_node << ".";
    os << lis.topic_name << " ";

    if (!lis.delegate_to.empty()) {
        // NEW: Print with brackets
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

// ---------------------------------------------------------
// Main Visitor
// ---------------------------------------------------------

void print_ast(const Program& p, std::ostream& os) {
    auto visitor = [&](auto&& x) {
        using T = std::decay_t<decltype(x)>;

        if constexpr (std::is_same_v<T, SystemModeDecl>) {
            os << "systemMode " << x.name << "\n";
        }
        else if constexpr (std::is_same_v<T, NodeDecl>) {
            os << "\nnode ";
            if (x.is_controller) os << "controller ";
            os << x.name << " : " << x.type_name;
            
            if (x.ignores_system) os << " ignore system";
            
            if (!x.config_text.empty()) os << " " << x.config_text;
            os << "\n";

            // Topics
            for (const auto& t : x.topics) {
                indent(os, 1);
                os << "topic " << t.name << " = \"" << t.path << "\" : ";
                print_type(t.type, os);
                os << "\n";
            }

            // Public Requests
            for (const auto& r : x.requests) {
                indent(os, 1);
                os << "onRequest ";
                if (!r.delegate_to.empty()) {
                    // NEW: Print with brackets
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

            // Global Listeners
            for (const auto& l : x.listeners) {
                print_listener(l, os, 1);
            }

            // Private Functions
            for (const auto& f : x.private_funcs) {
                indent(os, 1);
                os << "func " << f.sig.name;
                print_params(f.sig.params, os);
                os << " -> ";
                print_type(f.sig.return_type, os);
                os << "\n";
                print_stmts(f.body, os, 2);
            }
        }
        else if constexpr (std::is_same_v<T, ModeDecl>) {
            os << "\nmode " << x.node_name << "->";
            print_modename(x.mode_name, os);
            if (x.ignores_system) os << " ignore system";
            os << "\n";

            // Mode Body Statements
            print_stmts(x.body, os, 1);

            // Mode Scoped Listeners
            for (const auto& l : x.listeners) {
                print_listener(l, os, 1);
            }
        }
        else if constexpr (std::is_same_v<T, FuncDecl>) {
            os << "func " << x.sig.name << "\n";
        }
    };

    for (const auto& decl : p.decls) {
        std::visit(visitor, decl);
    }
}