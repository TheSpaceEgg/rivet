#include "print_ast.hpp"

#include <type_traits>

static void indent(std::ostream& os, int n) {
    for (int i = 0; i < n; ++i) os << "  ";
}

static void print_stmt(const Stmt& stmt, std::ostream& os, int depth) {
    std::visit([&](auto&& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, CallStmt>) {
            indent(os, depth);
            os << "Call " << s.callee << "(";
            for (size_t i = 0; i < s.args.size(); ++i) {
                os << s.args[i];
                if (i + 1 < s.args.size()) os << ", ";
            }
            os << ")\n";
        }
    }, stmt);
}

static void print_modename(const ModeName& m, std::ostream& os) {
    if (m.is_local_string) os << "\"" << m.text << "\"";
    else os << m.text;
}

void print_ast(const Program& program, std::ostream& os) {
    os << "Program\n";

    for (const auto& d : program.decls) {
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;

            if constexpr (std::is_same_v<T, SystemModeDecl>) {
                indent(os, 1);
                os << "SystemMode " << x.name << "\n";
            } else if constexpr (std::is_same_v<T, NodeDecl>) {
                indent(os, 1);
                os << "Node name=" << x.name << " type=" << x.type_name << "\n";
                if (!x.config_text.empty()) {
                    indent(os, 2);
                    os << "config=" << x.config_text << "\n";
                }
            } else if constexpr (std::is_same_v<T, ModeDecl>) {
                indent(os, 1);
                os << "Mode " << x.node_name << "->";
                print_modename(x.mode_name, os);
                os << "\n";

                if (x.delegate_to.has_value()) {
                    indent(os, 2);
                    os << "do ";
                    print_modename(*x.delegate_to, os);
                    os << "\n";
                }

                if (!x.body.empty()) {
                    indent(os, 2);
                    os << "body:\n";
                    for (const auto& st : x.body) print_stmt(st, os, 3);
                }
            }
        }, d);
    }
}
