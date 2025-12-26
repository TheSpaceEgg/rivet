#include "validate.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iostream>

struct TopicSymbol {
    TypeInfo type;
};

struct FuncSymbol {
    std::vector<TypeInfo> param_types;
    TypeInfo return_type;
};

struct NodeSymbol {
    std::string name;
    bool is_controller = false; 
    std::unordered_map<std::string, TopicSymbol> topics;     
    std::unordered_map<std::string, FuncSymbol> public_funcs; 
    std::unordered_map<std::string, FuncSymbol> private_funcs; 
};

static std::unordered_map<std::string, NodeSymbol> g_nodes;
static std::unordered_set<std::string> g_system_modes; 

static bool check_types(const TypeInfo& expected, const TypeInfo& actual) {
    if (expected.base != actual.base) return false;
    if (expected.base == ValType::Custom && expected.custom_name != actual.custom_name) return false;
    return true;
}

static ValType resolve_type(const std::string& val, const std::vector<Param>& current_params) {
    for (const auto& p : current_params) {
        if (p.name == val) return p.type.base;
    }

    if (val == "true" || val == "false") return ValType::Bool;
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') return ValType::String;
    
    if (!val.empty() && (isdigit(val[0]) || val[0] == '-')) {
        if (val.find('.') != std::string::npos) return ValType::Float;
        return ValType::Int;
    }

    return ValType::Int; 
}

static void collect_symbols(const Program& p, const DiagnosticEngine& diag) {
    g_nodes.clear();
    g_system_modes.clear();
    
    g_system_modes.insert("Init");
    g_system_modes.insert("Normal");
    g_system_modes.insert("Shutdown");

    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            NodeSymbol ns;
            ns.name = n->name;
            ns.is_controller = n->is_controller;

            for (const auto& t : n->topics) ns.topics[t.name] = { t.type };

            for (const auto& r : n->requests) {
                FuncSymbol fs;
                for (const auto& param : r.sig.params) fs.param_types.push_back(param.type);
                fs.return_type = r.sig.return_type;
                ns.public_funcs[r.sig.name] = fs;
            }

            for (const auto& f : n->private_funcs) {
                FuncSymbol fs;
                for (const auto& param : f.sig.params) fs.param_types.push_back(param.type);
                fs.return_type = f.sig.return_type;
                ns.private_funcs[f.sig.name] = fs;
            }

            if (g_nodes.find(n->name) != g_nodes.end()) {
                diag.error(n->loc, "Duplicate node definition '" + n->name + "'");
            }
            g_nodes[n->name] = ns;
        }
        else if (auto sm = std::get_if<SystemModeDecl>(&decl)) {
            if (sm->name == "Init" || sm->name == "Normal" || sm->name == "Shutdown") {
                diag.error(sm->loc, "System mode '" + sm->name + "' is reserved");
            }
            else if (g_system_modes.find(sm->name) != g_system_modes.end()) {
                diag.error(sm->loc, "Duplicate system mode declaration '" + sm->name + "'");
            }
            else {
                g_system_modes.insert(sm->name);
            }
        }
    }
}

static bool check_logic(const Program& p, const DiagnosticEngine& diag) {
    bool has_error = false;

    auto validate_stmts = [&](const std::vector<Stmt>& stmts, 
                              const std::string& current_node, 
                              const std::vector<Param>& current_params) {
        for (const auto& stmt : stmts) {
            
            // Validate Log Arguments (Variables)
            if (auto log = std::get_if<LogStmt>(&stmt)) {
                for (const auto& arg : log->args) {
                    // Skip literals
                    if (arg.size() >= 2 && arg.front() == '"') continue;
                    if (isdigit(arg[0]) || arg[0] == '-') continue;
                    if (arg == "true" || arg == "false") continue;

                    // Must be a parameter or topic (if inside node)
                    bool found = false;
                    for (const auto& p : current_params) {
                        if (p.name == arg) { found = true; break; }
                    }
                    if (!found && g_nodes.find(current_node) != g_nodes.end()) {
                         if (g_nodes[current_node].topics.count(arg)) found = true;
                    }

                    if (!found) {
                        diag.error(log->loc, "Unknown variable '" + arg + "' in log statement");
                        has_error = true;
                    }
                }
            }

            if (auto pub = std::get_if<PublishStmt>(&stmt)) {
                if (g_nodes.find(current_node) == g_nodes.end()) continue;
                
                auto& node_sym = g_nodes[current_node];
                if (node_sym.topics.find(pub->topic_handle) == node_sym.topics.end()) {
                    diag.error(pub->loc, "Unknown topic handle '" + pub->topic_handle + "' in node '" + current_node + "'");
                    has_error = true;
                    continue;
                }

                TypeInfo expected = node_sym.topics[pub->topic_handle].type;
                ValType actual_base = resolve_type(pub->value, current_params);
                
                if (expected.base != actual_base) {
                    diag.error(pub->loc, "Type mismatch in publish. Expected " + 
                               std::to_string((int)expected.base) + " got " + std::to_string((int)actual_base));
                    has_error = true;
                }
            }

            if (auto req = std::get_if<RequestStmt>(&stmt)) {
                if (g_nodes.find(req->target_node) == g_nodes.end()) {
                    diag.error(req->loc, "Unknown target node '" + req->target_node + "'");
                    has_error = true;
                    continue;
                }

                auto& target_sym = g_nodes[req->target_node];
                if (target_sym.public_funcs.find(req->func_name) == target_sym.public_funcs.end()) {
                    diag.error(req->loc, "Unknown function '" + req->func_name + "' on node '" + req->target_node + "'");
                    has_error = true;
                    continue;
                }

                auto& func_sym = target_sym.public_funcs[req->func_name];
                if (req->args.size() != func_sym.param_types.size()) {
                    diag.error(req->loc, "Argument count mismatch. Expected " + 
                               std::to_string(func_sym.param_types.size()) + ", got " + std::to_string(req->args.size()));
                    has_error = true;
                }
            }
            
            if (auto trans = std::get_if<TransitionStmt>(&stmt)) {
                if (trans->is_system) {
                    if (g_nodes.find(current_node) != g_nodes.end()) {
                        auto& node_sym = g_nodes[current_node];
                        if (!node_sym.is_controller) {
                            diag.error(trans->loc, "Node '" + current_node + "' is not a Controller. It cannot perform System Transitions.");
                            has_error = true;
                        }
                    }
                    if (g_system_modes.find(trans->target_state) == g_system_modes.end()) {
                         diag.error(trans->loc, "Unknown system mode '" + trans->target_state + "'");
                         has_error = true;
                    }
                }
            }

            if (auto call = std::get_if<CallStmt>(&stmt)) {
                 if (g_nodes.find(current_node) == g_nodes.end()) continue;
                 auto& ns = g_nodes[current_node];
                 if (ns.private_funcs.find(call->callee) == ns.private_funcs.end()) {
                      diag.error(call->loc, "Unknown private function '" + call->callee + "'");
                      has_error = true;
                 }
                 auto& func_sym = ns.private_funcs[call->callee];
                 if (call->args.size() != func_sym.param_types.size()) {
                     diag.error(call->loc, "Arg count mismatch in local call");
                     has_error = true;
                 }
            }
        }
    };

    auto validate_listener = [&](const OnListenDecl& lis, const std::string& current_node) {
         std::string src = lis.source_node.empty() ? current_node : lis.source_node;
         if (g_nodes.find(src) == g_nodes.end()) {
             diag.error(lis.loc, "Unknown node '" + src + "' in listener");
             has_error = true;
             return;
         }
         auto& src_node = g_nodes[src];
         if (src_node.topics.find(lis.topic_name) == src_node.topics.end()) {
             diag.error(lis.loc, "Unknown topic '" + lis.topic_name + "' on node '" + src + "'");
             has_error = true;
             return;
         }
         TypeInfo topicType = src_node.topics[lis.topic_name].type;

         if (!lis.delegate_to.empty()) {
             auto& my_node = g_nodes[current_node];
             if (my_node.private_funcs.find(lis.delegate_to) == my_node.private_funcs.end()) {
                 diag.error(lis.loc, "Cannot delegate to unknown function '" + lis.delegate_to + "'");
                 has_error = true;
                 return;
             }
             auto& fn = my_node.private_funcs[lis.delegate_to];
             if (fn.param_types.size() != 1) {
                 diag.error(lis.loc, "Delegated function must accept exactly 1 argument (the topic payload)");
                 has_error = true;
             } else if (!check_types(topicType, fn.param_types[0])) {
                 diag.error(lis.loc, "Type mismatch: Topic is " + std::to_string((int)topicType.base) + 
                                     " but function expects " + std::to_string((int)fn.param_types[0].base));
                 has_error = true;
             }
         } else {
             validate_stmts(lis.body, current_node, lis.sig.params);
         }
    };

    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            for (const auto& req : n->requests)      validate_stmts(req.body, n->name, req.sig.params);
            for (const auto& func : n->private_funcs) validate_stmts(func.body, n->name, func.sig.params);
            for (const auto& lis : n->listeners)     validate_listener(lis, n->name);
        }
        else if (auto m = std::get_if<ModeDecl>(&decl)) {
            validate_stmts(m->body, m->node_name, {});
            for (const auto& lis : m->listeners)     validate_listener(lis, m->node_name);
        }
    }

    return !has_error;
}

bool validate_program(const Program& program, const DiagnosticEngine& diag) {
    collect_symbols(program, diag);
    return check_logic(program, diag);
}