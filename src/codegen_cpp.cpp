#include "codegen_cpp.hpp"
#include <variant>
#include <string>

const char* RIVET_RUNTIME = R"(
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>

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
    std::vector<std::function<void(T)>> subscribers;
public:
    void publish(T val) { for (auto& cb : subscribers) cb(val); }
    void subscribe(std::function<void(T)> cb) { subscribers.push_back(cb); }
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

static void gen_stmts(const std::vector<Stmt>& stmts, std::ostream& os, int depth) {
    auto indent = [&](int d) { for (int i = 0; i < d; ++i) os << "    "; };
    for (const auto& stmt : stmts) {
        indent(depth);
        if (auto log = std::get_if<LogStmt>(&stmt)) {
            if (log->level == LogLevel::Print) {
                os << "std::cout";
                for (const auto& arg : log->args) os << " << " << arg;
                os << " << std::endl;\n";
            } else {
                std::string lvl = "LogLevel::INFO";
                if (log->level == LogLevel::Warn) lvl = "LogLevel::WARN";
                if (log->level == LogLevel::Error) lvl = "LogLevel::ERROR";
                if (log->level == LogLevel::Debug) lvl = "LogLevel::DEBUG";
                os << "Logger::log(this->name, " << lvl << ", std::string(\"\")";
                for (const auto& arg : log->args) {
                    if (!arg.empty() && arg[0] == '"') os << " + " << arg;
                    else os << " + std::to_string(" << arg << ")";
                }
                os << ");\n";
            }
        } else if (auto pub = std::get_if<PublishStmt>(&stmt)) {
            os << "this->" << pub->topic_handle << ".publish(" << pub->value << ");\n";
        } else if (auto tr = std::get_if<TransitionStmt>(&stmt)) {
            if (tr->is_system) os << "SystemManager::set_mode(\"" << tr->target_state << "\");\n";
            else os << "this->current_state = \"" << tr->target_state << "\";\n";
        } else if (auto req = std::get_if<RequestStmt>(&stmt)) {
            os << req->target_node << "_inst->" << req->func_name << "(";
            for (size_t i=0; i<req->args.size(); ++i) os << (i>0?", ":"") << req->args[i];
            os << ");\n";
        } else if (auto call = std::get_if<CallStmt>(&stmt)) {
            os << "this->" << call->callee << "(";
            for (size_t i=0; i<call->args.size(); ++i) os << (i>0?", ":"") << call->args[i];
            os << ");\n";
        } else if (auto ret = std::get_if<ReturnStmt>(&stmt)) {
            os << "return " << ret->value << ";\n";
        }
    }
}

void generate_cpp(const Program& p, std::ostream& os) {
    os << RIVET_RUNTIME << "\n";

    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            os << "class " << n->name << ";\nextern " << n->name << "* " << n->name << "_inst;\n";
        }
    }

    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            os << "\nclass " << n->name << " {\npublic:\n";
            os << "    std::string name = \"" << n->name << "\";\n";
            os << "    std::string current_state = \"Init\";\n";
            for (const auto& t : n->topics) 
                os << "    Topic<" << to_cpp_type(t.type) << "> " << t.name << ";\n";

            auto gen_func = [&](const FuncSignature& sig, const std::vector<Stmt>& body) {
                os << "    " << to_cpp_type(sig.return_type) << " " << sig.name << "(";
                for (size_t i=0; i<sig.params.size(); ++i) 
                    os << (i>0?", ":"") << to_cpp_type(sig.params[i].type) << " " << sig.params[i].name;
                os << ") {\n"; 
                gen_stmts(body, os, 2); 
                if (sig.return_type.base == ValType::Bool) os << "        return true;\n";
                os << "    }\n";
            };

            for (const auto& r : n->requests) gen_func(r.sig, r.body);
            for (const auto& f : n->private_funcs) gen_func(f.sig, f.body);

            // Handle Init Logic
            os << "    void init() {\n";
            for (const auto& d : p.decls) {
                if (auto m = std::get_if<ModeDecl>(&d))
                    if (m->node_name == n->name && m->mode_name.text == "Init") gen_stmts(m->body, os, 2);
            }
            os << "    }\n";

            // Handle System Mode Reaction
            os << "    void onSystemChange(std::string sys_mode) {\n";
            for (const auto& d : p.decls) {
                if (auto m = std::get_if<ModeDecl>(&d)) {
                    if (m->node_name == n->name && m->mode_name.text != "Init") {
                        os << "        if (sys_mode == \"" << m->mode_name.text << "\") {\n";
                        gen_stmts(m->body, os, 3);
                        os << "        }\n";
                    }
                }
            }
            os << "    }\n};\n";
            os << n->name << "* " << n->name << "_inst = nullptr;\n";
        }
    }

    os << "\nint main() {\n";
    for (const auto& decl : p.decls)
        if (auto n = std::get_if<NodeDecl>(&decl)) os << "    " << n->name << "_inst = new " << n->name << "();\n";

    // Setup Reaction Registry
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            os << "    SystemManager::on_transition.push_back([](std::string m) { " << n->name << "_inst->onSystemChange(m); });\n";
        }
    }

    // Wiring
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            for (const auto& l : n->listeners) {
                os << "    " << (l.source_node.empty()?n->name:l.source_node) << "_inst->" << l.topic_name << ".subscribe([=](auto val) {\n";
                if (l.delegate_to.empty()) gen_stmts(l.body, os, 2);
                else os << "        " << n->name << "_inst->" << l.delegate_to << "(val);\n";
                os << "    });\n";
            }
        }
        if (auto m = std::get_if<ModeDecl>(&decl)) {
            for (const auto& l : m->listeners) {
                os << "    " << (l.source_node.empty()?m->node_name:l.source_node) << "_inst->" << l.topic_name << ".subscribe([=](auto val) {\n";
                os << "        if (" << m->node_name << "_inst->current_state == \"" << m->mode_name.text << "\") {\n";
                if (l.delegate_to.empty()) gen_stmts(l.body, os, 3);
                else os << "            " << m->node_name << "_inst->" << l.delegate_to << "(val);\n";
                os << "        }\n    });\n";
            }
        }
    }

    for (const auto& decl : p.decls)
        if (auto n = std::get_if<NodeDecl>(&decl)) os << "    " << n->name << "_inst->init();\n";

    os << "    std::cout << \"--- Rivet System Started ---\" << std::endl;\n";
    os << "    while(true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }\n";
    os << "    return 0;\n}\n";
}