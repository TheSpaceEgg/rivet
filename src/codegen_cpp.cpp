#include "codegen_cpp.hpp"
#include <variant>
#include <string>
#include <regex>
#include <sstream>
#include <unordered_set>

const char* RIVET_RUNTIME = R"(
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <utility>

enum class LogLevel { INFO, WARN, ERROR, DEBUG };
struct Logger {
    static void log(const std::string& node, LogLevel level, const std::string& msg) {
        std::cout << "[" << node << "] ";
        switch(level) {
            case LogLevel::INFO:  std::cout << "[INFO] "; break;
            case LogLevel::WARN:  std::cout << "\033[33m[WARN]\033[0m "; break;
            case LogLevel::ERROR: std::cout << "\033[31m[ERROR]\033[0m "; break;
            case LogLevel::DEBUG: std::cout << "\033[36m[DEBUG]\033[0m "; break;
        }
        std::cout << msg << std::endl;
    }
};

template <typename T>
class Topic {
    struct Sub {
        int id;
        std::function<void(T)> cb;
    };
    std::vector<Sub> subscribers;
    int next_id = 1;
public:
    void publish(T val) {
        for (auto& s : subscribers) {
            if (s.cb) s.cb(val);
        }
    }

    // Returns a subscription handle that can be used to unsubscribe.
    int subscribe(std::function<void(T)> cb) {
        int id = next_id++;
        subscribers.push_back(Sub{id, std::move(cb)});
        return id;
    }

    void unsubscribe(int id) {
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                           [&](const Sub& s) { return s.id == id; }),
            subscribers.end());
    }
};

class SystemManager {
public:
    static std::string current_mode;
    static std::vector<std::function<void(std::string)>> on_transition;
    static void set_mode(const std::string& m) {
        if (current_mode != m) {
            std::cout << "[SYS] Transitioning to: " << m << std::endl;
            current_mode = m;
            for (auto& cb : on_transition) cb(m);
        }
    }
};
std::string SystemManager::current_mode = "Init";
std::vector<std::function<void(std::string)>> SystemManager::on_transition;
)";

static std::string to_cpp_type(const TypeInfo& t) {
    switch(t.base) {
        case ValType::Int:    return "int";
        case ValType::Float:  return "double"; 
        case ValType::String: return "std::string";
        case ValType::Bool:   return "bool";
        default:              return "void";
    }
}

static void gen_interpolated_string(const std::string& input, std::ostream& os) {
    std::regex re("\\{([^}]+)\\}");
    std::string s = input;
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);

    auto it = std::sregex_iterator(s.begin(), s.end(), re);
    auto end = std::sregex_iterator();
    size_t last_pos = 0;

    for (; it != end; ++it) {
        std::smatch match = *it;
        if ((size_t)match.position() > last_pos) {
            os << " << \"" << s.substr(last_pos, match.position() - last_pos) << "\"";
        }
        os << " << " << match.str(1);
        last_pos = match.position() + match.length();
    }
    if (last_pos < s.size()) os << " << \"" << s.substr(last_pos) << "\"";
}

static void gen_expr(const ExprPtr& e, std::ostream& os) {
    if (!e) { os << "0"; return; }

    if (auto lit = std::get_if<Expr::Literal>(&e->v)) {
        os << lit->text;
        return;
    }
    if (auto id = std::get_if<Expr::Ident>(&e->v)) {
        os << id->name;
        return;
    }
    if (auto call = std::get_if<Expr::Call>(&e->v)) {
            // Builtins: emit directly.
            if (call->callee == "min" || call->callee == "max") {
                const char* fn = call->callee == "min" ? "std::min" : "std::max";
                os << fn << "<double>(";
                if (call->args.size() >= 1) {
                    os << "(double)(";
                    gen_expr(call->args[0], os);
                    os << ")";
                } else {
                    os << "(double)0";
                }
                os << ", ";
                if (call->args.size() >= 2) {
                    os << "(double)(";
                    gen_expr(call->args[1], os);
                    os << ")";
                } else {
                    os << "(double)0";
                }
                os << ")";
                return;
            }

            if (call->callee == "clamp") {
                os << "std::clamp<double>(";

                if (call->args.size() >= 1) {
                    os << "(double)(";
                    gen_expr(call->args[0], os);
                    os << ")";
                } else {
                    os << "(double)0";
                }

                os << ", ";

                if (call->args.size() >= 2) {
                    os << "(double)(";
                    gen_expr(call->args[1], os);
                    os << ")";
                } else {
                    os << "(double)0";
                }

                os << ", ";

                if (call->args.size() >= 3) {
                    os << "(double)(";
                    gen_expr(call->args[2], os);
                    os << ")";
                } else {
                    os << "(double)0";
                }

                os << ")";
                return;
            }

            // Default: emit as a normal call expression.
            os << call->callee << "(";
            for (size_t i = 0; i < call->args.size(); ++i) {
                if (i) os << ", ";
                gen_expr(call->args[i], os);
            }
            os << ")";
            return;
        }

    if (auto un = std::get_if<Expr::Unary>(&e->v)) {
        os << "(";
        if (un->op == UnaryOp::Not) os << "!";
        else os << "-";
        gen_expr(un->rhs, os);
        os << ")";
        return;
    }
    if (auto bin = std::get_if<Expr::Binary>(&e->v)) {
        const char* op = "?";
        switch (bin->op) {
            case BinaryOp::Add: op = "+"; break;
            case BinaryOp::Sub: op = "-"; break;
            case BinaryOp::Mul: op = "*"; break;
            case BinaryOp::Div: op = "/"; break;
            case BinaryOp::Mod: op = "%"; break;
            case BinaryOp::Eq:  op = "=="; break;
            case BinaryOp::Neq: op = "!="; break;
            case BinaryOp::Lt:  op = "<"; break;
            case BinaryOp::Lte: op = "<="; break;
            case BinaryOp::Gt:  op = ">"; break;
            case BinaryOp::Gte: op = ">="; break;
            case BinaryOp::And: op = "&&"; break;
            case BinaryOp::Or:  op = "||"; break;
        }
        os << "(";
        gen_expr(bin->lhs, os);
        os << " " << op << " ";
        gen_expr(bin->rhs, os);
        os << ")";
        return;
    }
    os << "0";
}

static void gen_stmts(const std::vector<StmtPtr>& stmts, std::ostream& os, int depth) {
    auto indent = [&](int d) { for (int i = 0; i < d; ++i) os << "    "; };

    for (const auto& sp : stmts) {
        if (!sp) continue;

        // If statement needs its own indentation management because it expands to blocks.
        if (auto ifs = std::get_if<IfStmt>(&sp->v)) {
            indent(depth);
            os << "if ("; gen_expr(ifs->cond, os); os << ") {\n";
            gen_stmts(ifs->then_body, os, depth + 1);
            indent(depth);
            os << "}";

            for (const auto& br : ifs->elifs) {
                os << " else if ("; gen_expr(br.cond, os); os << ") {\n";
                gen_stmts(br.body, os, depth + 1);
                indent(depth);
                os << "}";
            }

            if (!ifs->else_body.empty()) {
                os << " else {\n";
                gen_stmts(ifs->else_body, os, depth + 1);
                indent(depth);
                os << "}";
            }
            os << "\n";
            continue;
        }

        indent(depth);

        if (auto log = std::get_if<LogStmt>(&sp->v)) {
            if (log->level == LogLevel::Print) {
                os << "std::cout";
                for (const auto& arg : log->args) {
                    if (!arg.empty() && arg[0] == '"') gen_interpolated_string(arg, os);
                    else os << " << " << arg;
                }
                os << " << std::endl;\n";
            } else {
                std::string lvl = "LogLevel::INFO";
                if (log->level == LogLevel::Warn) lvl = "LogLevel::WARN";
                if (log->level == LogLevel::Error) lvl = "LogLevel::ERROR";
                if (log->level == LogLevel::Debug) lvl = "LogLevel::DEBUG";
                os << "{ std::stringstream _ss; _ss";
                for (const auto& arg : log->args) {
                    if (!arg.empty() && arg[0] == '"') gen_interpolated_string(arg, os);
                    else os << " << " << arg;
                }
                os << "; Logger::log(this->name, " << lvl << ", _ss.str()); }\n";
            }
        } else if (auto pub = std::get_if<PublishStmt>(&sp->v)) {
            os << "this->" << pub->topic_handle << ".publish(" << pub->value << ");\n";
        } else if (auto tr = std::get_if<TransitionStmt>(&sp->v)) {
            if (tr->is_system) {
                os << "SystemManager::set_mode(\"" << tr->target_state << "\");\n";
            } else if (!tr->target_node.empty()) {
                os << tr->target_node << "_inst->set_state(\"" << tr->target_state << "\");\n";
            } else {
                os << "this->set_state(\"" << tr->target_state << "\");\n";
            }
        } else if (auto req = std::get_if<RequestStmt>(&sp->v)) {
            os << req->target_node << "_inst->" << req->func_name << "(";
            for (size_t i = 0; i < req->args.size(); ++i) os << (i > 0 ? ", " : "") << req->args[i];
            os << ");\n";
        } else if (auto call = std::get_if<CallStmt>(&sp->v)) {
            os << "this->" << call->callee << "(";
            for (size_t i = 0; i < call->args.size(); ++i) os << (i > 0 ? ", " : "") << call->args[i];
            os << ");\n";
        } else if (auto ret = std::get_if<ReturnStmt>(&sp->v)) {
            os << "return " << ret->value << ";\n";
        }
    }
}

void generate_cpp(const Program& p, std::ostream& os) {
    std::unordered_set<std::string> system_modes;
    for (const auto& d : p.decls) {
        if (auto sm = std::get_if<SystemModeDecl>(&d)) system_modes.insert(sm->name);
    }

    os << RIVET_RUNTIME << "\n";
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            os << "class " << n->name << ";\nextern " << n->name << "* " << n->name << "_inst;\n";
        }
    }
    // Pass 1: class declarations (no method bodies). This avoids C++ incomplete-type
    // issues when one node calls into another node declared later.
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            // Collect mode declarations for this node so we can declare subscription handles.
            std::vector<const ModeDecl*> node_modes;
            node_modes.reserve(p.decls.size());
            for (const auto& d2 : p.decls) {
                if (auto m2 = std::get_if<ModeDecl>(&d2)) {
                    if (m2->node_name == n->name) node_modes.push_back(m2);
                }
            }

            auto sub_name = [&](int mi, int li) {
                return std::string("__rivet_sub_m") + std::to_string(mi) + "_l" + std::to_string(li);
            };

            os << "\nclass " << n->name << " {\npublic:\n";
            os << "    std::string name = \"" << n->name << "\";\n";
            os << "    std::string current_state = \"Init\";\n";
            for (const auto& t : n->topics) os << "    Topic<" << to_cpp_type(t.type) << "> " << t.name << ";\n";

            // Mode-scoped subscription handles (for onListen inside mode blocks)
            for (int mi = 0; mi < (int)node_modes.size(); ++mi) {
                for (int li = 0; li < (int)node_modes[mi]->listeners.size(); ++li) {
                    os << "    int " << sub_name(mi, li) << " = -1;\n";
                }
            }

            // Requests + functions
            auto decl_func = [&](const FuncSignature& sig) {
                os << "    " << to_cpp_type(sig.return_type) << " " << sig.name << "(";
                for (size_t i = 0; i < sig.params.size(); ++i) {
                    if (i) os << ", ";
                    os << to_cpp_type(sig.params[i].type) << " " << sig.params[i].name;
                }
                os << ");\n";
            };
            for (const auto& r : n->requests) decl_func(r.sig);
            for (const auto& f : n->private_funcs) decl_func(f.sig);

            // Lifecycle / transition hooks
            os << "    void init();\n";
            os << "    void onSystemChange(std::string sys_mode);\n";
            os << "    void onLocalChange();\n";
            os << "    void set_state(const std::string& st);\n";
            os << "    void __rivet_unsub_sys_listeners();\n";
            os << "    void __rivet_unsub_local_listeners();\n";

            os << "};\n";
            os << n->name << "* " << n->name << "_inst = nullptr;\n";
        }
    }

    // Pass 2: method definitions (after all classes exist).
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            // Collect mode declarations for this node.
            std::vector<const ModeDecl*> node_modes;
            node_modes.reserve(p.decls.size());
            for (const auto& d2 : p.decls) {
                if (auto m2 = std::get_if<ModeDecl>(&d2)) {
                    if (m2->node_name == n->name) node_modes.push_back(m2);
                }
            }

            auto is_system_mode = [&](const ModeDecl* m) -> bool {
                if (!m) return false;
                if (m->mode_name.text == "Init") return false;
                if (m->mode_name.is_local_string) return false;
                if (m->ignores_system) return false;
                return system_modes.find(m->mode_name.text) != system_modes.end();
            };
            auto is_local_mode = [&](const ModeDecl* m) -> bool {
                if (!m) return false;
                if (m->mode_name.text == "Init") return false;
                if (m->mode_name.is_local_string) return true;
                if (m->ignores_system) return true;
                return system_modes.find(m->mode_name.text) == system_modes.end();
            };

            auto sub_name = [&](int mi, int li) {
                return std::string("__rivet_sub_m") + std::to_string(mi) + "_l" + std::to_string(li);
            };

            auto emit_subscribe = [&](const std::string& owner_node,
                                      const OnListenDecl& l,
                                      const std::string& subvar,
                                      int depth) {
                auto indent = [&](int d) { for (int i = 0; i < d; ++i) os << "    "; };
                std::string src = l.source_node.empty() ? owner_node : l.source_node;

                indent(depth);
                os << "if (" << subvar << " == -1) " << subvar << " = "
                   << src << "_inst->" << l.topic_name
                   << ".subscribe([this](auto val) {\n";

                if (l.delegate_to.empty()) {
                    gen_stmts(l.body, os, depth + 1);
                } else {
                    indent(depth + 1);
                    os << "this->" << l.delegate_to << "(val);\n";
                }

                indent(depth);
                os << "});\n";
            };

            auto gen_method = [&](const FuncSignature& sig, const std::vector<StmtPtr>& body) {
                os << "\n" << to_cpp_type(sig.return_type) << " " << n->name << "::" << sig.name << "(";
                for (size_t i = 0; i < sig.params.size(); ++i) {
                    if (i) os << ", ";
                    os << to_cpp_type(sig.params[i].type) << " " << sig.params[i].name;
                }
                os << ") {\n";
                gen_stmts(body, os, 1);
                bool has_return = false;
                for (const auto& st : body) {
                    if (st && std::holds_alternative<ReturnStmt>(st->v)) { has_return = true; break; }
                }
                if (sig.return_type.base == ValType::Bool && !has_return) os << "    return true;\n";
                os << "}\n";
            };
            for (const auto& r : n->requests) gen_method(r.sig, r.body);
            for (const auto& f : n->private_funcs) gen_method(f.sig, f.body);

            // Unsubscribe helpers
            os << "\nvoid " << n->name << "::__rivet_unsub_sys_listeners() {\n";
            for (int mi = 0; mi < (int)node_modes.size(); ++mi) {
                const auto* m = node_modes[mi];
                if (!is_system_mode(m)) continue;
                for (int li = 0; li < (int)m->listeners.size(); ++li) {
                    const auto& l = m->listeners[li];
                    std::string src = l.source_node.empty() ? n->name : l.source_node;
                    std::string sub = sub_name(mi, li);
                    os << "    if (" << sub << " != -1) { "
                       << src << "_inst->" << l.topic_name << ".unsubscribe(" << sub << "); "
                       << sub << " = -1; }\n";
                }
            }
            os << "}\n";

            os << "\nvoid " << n->name << "::__rivet_unsub_local_listeners() {\n";
            for (int mi = 0; mi < (int)node_modes.size(); ++mi) {
                const auto* m = node_modes[mi];
                if (!is_local_mode(m)) continue;
                for (int li = 0; li < (int)m->listeners.size(); ++li) {
                    const auto& l = m->listeners[li];
                    std::string src = l.source_node.empty() ? n->name : l.source_node;
                    std::string sub = sub_name(mi, li);
                    os << "    if (" << sub << " != -1) { "
                       << src << "_inst->" << l.topic_name << ".unsubscribe(" << sub << "); "
                       << sub << " = -1; }\n";
                }
            }
            os << "}\n";

            // init
            os << "\nvoid " << n->name << "::init() {\n";
            os << "    this->__rivet_unsub_sys_listeners();\n";
            os << "    this->__rivet_unsub_local_listeners();\n";
            for (int mi = 0; mi < (int)node_modes.size(); ++mi) {
                const auto* m = node_modes[mi];
                if (m->mode_name.text != "Init") continue;
                for (int li = 0; li < (int)m->listeners.size(); ++li) {
                    emit_subscribe(n->name, m->listeners[li], sub_name(mi, li), 2);
                }
                gen_stmts(m->body, os, 2);
            }
            os << "}\n";

            // system change
            os << "\nvoid " << n->name << "::onSystemChange(std::string sys_mode) {\n";
            if (n->ignores_system) {
                os << "    (void)sys_mode;\n";
                os << "    return;\n";
            } else {
                os << "    this->__rivet_unsub_sys_listeners();\n";
                for (int mi = 0; mi < (int)node_modes.size(); ++mi) {
                    const auto* m = node_modes[mi];
                    if (!is_system_mode(m)) continue;
                    os << "    if (sys_mode == \"" << m->mode_name.text << "\") {\n";
                    for (int li = 0; li < (int)m->listeners.size(); ++li) {
                        emit_subscribe(n->name, m->listeners[li], sub_name(mi, li), 2);
                    }
                    gen_stmts(m->body, os, 2);
                    os << "    }\n";
                }
            }
            os << "}\n";

            // local change
            os << "\nvoid " << n->name << "::onLocalChange() {\n";
            os << "    this->__rivet_unsub_local_listeners();\n";
            for (int mi = 0; mi < (int)node_modes.size(); ++mi) {
                const auto* m = node_modes[mi];
                if (!is_local_mode(m)) continue;
                os << "    if (this->current_state == \"" << m->mode_name.text << "\") {\n";
                for (int li = 0; li < (int)m->listeners.size(); ++li) {
                    emit_subscribe(n->name, m->listeners[li], sub_name(mi, li), 2);
                }
                gen_stmts(m->body, os, 2);
                os << "    }\n";
            }
            os << "}\n";

            // set_state
            os << "\nvoid " << n->name << "::set_state(const std::string& st) {\n";
            os << "    this->current_state = st;\n";
            os << "    this->onLocalChange();\n";
            os << "}\n";
        }
    }
    os << "\nint main() {\n";
    for (const auto& decl : p.decls)
        if (auto n = std::get_if<NodeDecl>(&decl)) os << "    " << n->name << "_inst = new " << n->name << "();\n";
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            if (!n->ignores_system) {
                os << "    SystemManager::on_transition.push_back([](std::string m) { "
                   << n->name << "_inst->onSystemChange(m); });\n";
            }
        }
    }
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            for (const auto& l : n->listeners) {
                os << "    " << (l.source_node.empty()?n->name:l.source_node) << "_inst->" << l.topic_name << ".subscribe([=](auto val) {\n";
                if (l.delegate_to.empty()) gen_stmts(l.body, os, 2);
                else os << "        " << n->name << "_inst->" << l.delegate_to << "(val);\n";
                os << "    });\n";
            }
        }
    }
    for (const auto& decl : p.decls)
        if (auto n = std::get_if<NodeDecl>(&decl)) os << "    " << n->name << "_inst->init();\n";
    os << "    std::cout << \"--- Rivet System Started ---\" << std::endl;\n";
    os << "    while(true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }\n    return 0;\n}\n";
}