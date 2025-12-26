#include "print_ast.hpp"
#include <type_traits>
#include <string>

static void indent(std::ostream& os, int n) {
    for (int i = 0; i < n; ++i) os << "  ";
}

static std::string type_str(const TypeInfo& t) {
    switch(t.base) {
        case ValType::Int: return "int";
        case ValType::Float: return "float";
        case ValType::String: return "string";
        case ValType::Bool: return "bool";
        case ValType::Custom: return t.custom_name;
    }
    return "unknown";
}

static void print_stmt(const Stmt& stmt, std::ostream& os, int depth) {
    std::visit([&](auto&& s) {
        using T = std::decay_t<decltype(s)>;
        indent(os, depth);
        if constexpr (std::is_same_v<T, CallStmt>) {
            os << "Call " << s.callee << "(";
            for (size_t i = 0; i < s.args.size(); ++i) {
                os << s.args[i];
                if (i + 1 < s.args.size()) os << ", ";
            }
            os << ")\n";
        }
        else if constexpr (std::is_same_v<T, RequestStmt>) {
            os << "Request " << (s.is_silent ? "(silent) " : "")
               << s.target_node << "." << s.func_name << "(";
            for (size_t i = 0; i < s.args.size(); ++i) {
                os << s.args[i];
                if (i + 1 < s.args.size()) os << ", ";
            }
            os << ")\n";
        }
        else if constexpr (std::is_same_v<T, PublishStmt>) {
            os << "Publish handle=" << s.topic_handle << " value=" << s.value << "\n";
        }
        else if constexpr (std::is_same_v<T, ReturnStmt>) {
            os << "Return " << s.value << "\n";
        }
    }, stmt);
}

static void print_modename(const ModeName& m, std::ostream& os) {
    if (m.is_local_string) os << "\"" << m.text << "\"";
    else os << m.text;
}

static void print_signature(const FuncSignature& sig, std::ostream& os) {
    os << sig.name << "(";
    for (size_t i = 0; i < sig.params.size(); ++i) {
        os << sig.params[i].name << ": " << type_str(sig.params[i].type);
        if (i + 1 < sig.params.size()) os << ", ";
    }
    os << ") -> " << type_str(sig.return_type);
}

void print_ast(const Program& program, std::ostream& os) {
    os << "Program\n";

    for (const auto& d : program.decls) {
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;

            if constexpr (std::is_same_v<T, SystemModeDecl>) {
                indent(os, 1);
                os << "SystemMode " << x.name << "\n";
            }
            else if constexpr (std::is_same_v<T, FuncDecl>) {
                indent(os, 1);
                os << "Func ";
                print_signature(x.sig, os);
                os << "\n";
                for (const auto& st : x.body) print_stmt(st, os, 2);
            }
            else if constexpr (std::is_same_v<T, NodeDecl>) {
                indent(os, 1);
                os << "Node name=" << x.name << " type=" << x.type_name << "\n";
                if (!x.config_text.empty()) {
                    indent(os, 2);
                    os << "config=" << x.config_text << "\n";
                }

                // TOPICS
                for (const auto& topic : x.topics) {
                    indent(os, 2);
                    os << "Topic handle=" << topic.name 
                       << " path=\"" << topic.path << "\"" 
                       << " type=" << type_str(topic.type) << "\n";
                }

                // PRIVATES
                for (const auto& func : x.private_funcs) {
                    indent(os, 2);
                    os << "PrivateFunc ";
                    print_signature(func.sig, os);
                    os << "\n";
                    for (const auto& st : func.body) print_stmt(st, os, 3);
                }

                // REQUESTS
                for (const auto& req : x.requests) {
                    indent(os, 2);
                    os << "OnRequest ";
                    if (!req.delegate_to.empty()) {
                         os << "(Delegated to " << req.delegate_to << ")\n";
                    } else {
                         print_signature(req.sig, os);
                         os << "\n";
                         for (const auto& st : req.body) print_stmt(st, os, 3);
                    }
                }

                // LISTENERS
                for (const auto& lis : x.listeners) {
                    indent(os, 2);
                    os << "OnListen handle=";
                    if (!lis.source_node.empty()) os << lis.source_node << ".";
                    os << lis.topic_name << " ";
                    
                    if (!lis.delegate_to.empty()) {
                        os << "Delegate -> " << lis.delegate_to << "\n";
                    } else {
                        print_signature(lis.sig, os);
                        os << "\n";
                        for (const auto& st : lis.body) print_stmt(st, os, 3);
                    }
                }
            } 
            else if constexpr (std::is_same_v<T, ModeDecl>) {
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