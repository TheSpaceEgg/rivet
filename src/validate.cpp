#include "validate.hpp"
#include "builtins.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <string_view>
#include <vector>
#include <functional>
#include <cctype>

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
// Mode tables used for validating local/cross-node transitions.
static std::unordered_map<std::string, std::unordered_set<std::string>> g_any_modes_by_node;
static std::unordered_map<std::string, std::unordered_set<std::string>> g_local_modes_by_node;

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

static std::string trim_copy(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && std::isspace((unsigned char)sv[b])) b++;
    size_t e = sv.size();
    while (e > b && std::isspace((unsigned char)sv[e - 1])) e--;
    return std::string(sv.substr(b, e - b));
}

static bool is_simple_ident(std::string_view sv) {
    if (sv.empty()) return false;
    auto is_start = [&](unsigned char c) { return std::isalpha(c) || c == '_'; };
    auto is_body = [&](unsigned char c) { return std::isalnum(c) || c == '_'; };
    if (!is_start((unsigned char)sv[0])) return false;
    for (size_t i = 1; i < sv.size(); ++i) {
        if (!is_body((unsigned char)sv[i])) return false;
    }
    return true;
}

static std::vector<std::string> extract_interpolations(const std::string& s) {
    // Input includes quotes ("...") because lexer preserves them for codegen.
    // We only validate simple identifiers inside {braces}; complex expressions are ignored.
    std::vector<std::string> out;
    size_t i = 0;
    while (true) {
        size_t l = s.find('{', i);
        if (l == std::string::npos) break;
        size_t r = s.find('}', l + 1);
        if (r == std::string::npos) break;
        std::string inner = trim_copy(std::string_view(s).substr(l + 1, r - l - 1));
        if (!inner.empty()) out.push_back(std::move(inner));
        i = r + 1;
    }
    return out;
}

static void collect_symbols(const Program& p, const DiagnosticEngine& diag) {
    g_nodes.clear();
    g_system_modes.clear();
    g_any_modes_by_node.clear();
    g_local_modes_by_node.clear();
    
    g_system_modes.insert("Init");
    g_system_modes.insert("Normal");
    g_system_modes.insert("Shutdown");

    // Pass 1: collect system modes (so we can correctly classify local modes later).
    for (const auto& decl : p.decls) {
        if (auto sm = std::get_if<SystemModeDecl>(&decl)) {
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

    // Pass 2: collect nodes and function/topic symbols.
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
    }

    // Pass 3: collect per-node mode names and classify local modes.
    for (const auto& decl : p.decls) {
        if (auto m = std::get_if<ModeDecl>(&decl)) {
            g_any_modes_by_node[m->node_name].insert(m->mode_name.text);
            bool is_local = m->mode_name.is_local_string || m->ignores_system ||
                            (g_system_modes.find(m->mode_name.text) == g_system_modes.end());
            if (is_local) g_local_modes_by_node[m->node_name].insert(m->mode_name.text);
        }
    }
}

static bool check_logic(const Program& p, const DiagnosticEngine& diag) {
    bool has_error = false;

    auto is_numeric = [](ValType t) { return t == ValType::Int || t == ValType::Float; };

    auto infer_expr = [&](auto&& self,
                          const ExprPtr& e,
                          const std::string& current_node,
                          const std::vector<Param>& current_params) -> ValType {
        if (!e) return ValType::Int;

        if (auto lit = std::get_if<Expr::Literal>(&e->v)) {
            switch (lit->kind) {
                case Expr::Literal::Kind::Int: return ValType::Int;
                case Expr::Literal::Kind::Float: return ValType::Float;
                case Expr::Literal::Kind::String: return ValType::String;
                case Expr::Literal::Kind::Bool: return ValType::Bool;
            }
        }
        if (auto id = std::get_if<Expr::Ident>(&e->v)) {
            for (const auto& p : current_params) {
                if (p.name == id->name) return p.type.base;
            }
            auto itn = g_nodes.find(current_node);
            if (itn != g_nodes.end()) {
                auto itt = itn->second.topics.find(id->name);
                if (itt != itn->second.topics.end()) return itt->second.type.base;
            }
            diag.error(e->loc, "Unknown identifier '" + id->name + "' in expression");
            has_error = true;
            return ValType::Int;
        }
        if (auto call = std::get_if<Expr::Call>(&e->v)) {
            std::vector<ValType> arg_types;
            arg_types.reserve(call->args.size());
            for (const auto& a : call->args) {
                arg_types.push_back(self(self, a, current_node, current_params));
            }

            // Builtins (min/max/...) live outside any node.
            if (const BuiltinId* bid = lookup_builtin(call->callee)) {
                auto promote_num = [&](ValType a, ValType b) {
                    return (a == ValType::Float || b == ValType::Float) ? ValType::Float : ValType::Int;
                };

                switch (*bid) {
                    case BuiltinId::Min:
                    case BuiltinId::Max: {
                        if (arg_types.size() != 2) {
                            diag.error(e->loc, "Builtin '" + call->callee + "' expects 2 arguments");
                            has_error = true;
                            return ValType::Int;
                        }
                        if (!is_numeric(arg_types[0]) || !is_numeric(arg_types[1])) {
                            diag.error(e->loc, "Builtin '" + call->callee + "' requires numeric arguments");
                            has_error = true;
                            return ValType::Int;
                        }
                        return promote_num(arg_types[0], arg_types[1]);
                    }

                    case BuiltinId::Clamp: {
                        if (arg_types.size() != 3) {
                            diag.error(e->loc, "Builtin '" + call->callee + "' expects 3 arguments");
                            has_error = true;
                            return ValType::Int;
                        }
                        if (!is_numeric(arg_types[0]) || !is_numeric(arg_types[1]) || !is_numeric(arg_types[2])) {
                            diag.error(e->loc, "Builtin '" + call->callee + "' requires numeric arguments");
                            has_error = true;
                            return ValType::Int;
                        }
                        return promote_num(promote_num(arg_types[0], arg_types[1]), arg_types[2]);
                    }
                }
            }

            // Node function call expression (e.g., helper(...) used inside an if condition)
            auto itn = g_nodes.find(current_node);
            if (itn != g_nodes.end()) {
                const FuncSymbol* fs = nullptr;
                auto ip = itn->second.private_funcs.find(call->callee);
                if (ip != itn->second.private_funcs.end()) fs = &ip->second;
                auto iu = itn->second.public_funcs.find(call->callee);
                if (!fs && iu != itn->second.public_funcs.end()) fs = &iu->second;

                if (fs) {
                    if (arg_types.size() != fs->param_types.size()) {
                        diag.error(e->loc, "Argument count mismatch in call to '" + call->callee + "'. Expected " +
                                   std::to_string(fs->param_types.size()) + ", got " + std::to_string(arg_types.size()));
                        has_error = true;
                        return fs->return_type.base;
                    }
                    for (size_t i = 0; i < arg_types.size() && i < fs->param_types.size(); ++i) {
                        ValType expected = fs->param_types[i].base;
                        ValType actual = arg_types[i];
                        bool ok = (expected == actual) || (expected == ValType::Float && actual == ValType::Int);
                        if (!ok) {
                            diag.error(e->loc, "Type mismatch in call to '" + call->callee + "' argument " + std::to_string(i) + "");
                            has_error = true;
                        }
                    }
                    return fs->return_type.base;
                }
            }

            diag.error(e->loc, "Unknown function '" + call->callee + "' in expression");
            has_error = true;
            return ValType::Int;
        }
        if (auto un = std::get_if<Expr::Unary>(&e->v)) {
            ValType rhs = self(self, un->rhs, current_node, current_params);
            if (un->op == UnaryOp::Not) {
                if (rhs != ValType::Bool) {
                    diag.error(e->loc, "Unary 'not' requires a bool operand");
                    has_error = true;
                }
                return ValType::Bool;
            }
            if (un->op == UnaryOp::Neg) {
                if (!is_numeric(rhs)) {
                    diag.error(e->loc, "Unary '-' requires a numeric operand");
                    has_error = true;
                }
                return rhs;
            }
        }
        if (auto bin = std::get_if<Expr::Binary>(&e->v)) {
            ValType lt = self(self, bin->lhs, current_node, current_params);
            ValType rt = self(self, bin->rhs, current_node, current_params);

            auto promote_num = [&](ValType a, ValType b) {
                return (a == ValType::Float || b == ValType::Float) ? ValType::Float : ValType::Int;
            };

            switch (bin->op) {
                case BinaryOp::Add:
                case BinaryOp::Sub:
                case BinaryOp::Mul:
                case BinaryOp::Div:
                case BinaryOp::Mod: {
                    if (!is_numeric(lt) || !is_numeric(rt)) {
                        diag.error(e->loc, "Arithmetic operator requires numeric operands");
                        has_error = true;
                        return ValType::Int;
                    }
                    return promote_num(lt, rt);
                }
                case BinaryOp::Eq:
                case BinaryOp::Neq: {
                    // Allow numeric equality across int/float with implicit promotion.
                    if (lt != rt) {
                        if (!(is_numeric(lt) && is_numeric(rt))) {
                            diag.error(e->loc, "Equality operator requires operands of compatible types");
                            has_error = true;
                        }
                    }
                    return ValType::Bool;
                }
                case BinaryOp::Lt:
                case BinaryOp::Lte:
                case BinaryOp::Gt:
                case BinaryOp::Gte: {
                    if (!is_numeric(lt) || !is_numeric(rt)) {
                        diag.error(e->loc, "Comparison operator requires numeric operands");
                        has_error = true;
                    }
                    return ValType::Bool;
                }
                case BinaryOp::And:
                case BinaryOp::Or: {
                    if (lt != ValType::Bool || rt != ValType::Bool) {
                        diag.error(e->loc, "Boolean operator requires bool operands");
                        has_error = true;
                    }
                    return ValType::Bool;
                }
            }
        }
        return ValType::Int;
    };

    std::function<void(const std::vector<StmtPtr>&,
                       const std::string&,
                       const std::vector<Param>&)> validate_stmts;

    validate_stmts = [&](const std::vector<StmtPtr>& stmts,
                         const std::string& current_node,
                         const std::vector<Param>& current_params) {
        for (const auto& sp : stmts) {
            if (!sp) continue;

            // Validate Log Arguments (Variables)
            if (auto log = std::get_if<LogStmt>(&sp->v)) {
                for (const auto& arg : log->args) {
                    auto check_var = [&](const std::string& name) {
                        if (name.empty()) return;
                        if (name == "true" || name == "false") return;

                        bool found = false;
                        for (const auto& p : current_params) {
                            if (p.name == name) { found = true; break; }
                        }
                        if (!found && g_nodes.find(current_node) != g_nodes.end()) {
                            if (g_nodes[current_node].topics.count(name)) found = true;
                        }
                        if (!found) {
                            diag.error(log->loc, "Unknown variable '" + name + "' in log statement");
                            has_error = true;
                        }
                    };

                    if (arg.size() >= 2 && arg.front() == '"') {
                        for (const auto& inner : extract_interpolations(arg)) {
                            if (is_simple_ident(inner)) check_var(inner);
                        }
                        continue;
                    }

                    if (!arg.empty() && (isdigit((unsigned char)arg[0]) || arg[0] == '-')) continue;
                    check_var(arg);
                }
            }

            if (auto pub = std::get_if<PublishStmt>(&sp->v)) {
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

            if (auto req = std::get_if<RequestStmt>(&sp->v)) {
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

            if (auto trans = std::get_if<TransitionStmt>(&sp->v)) {
                if (trans->is_system) {
                    // System transitions require a controller and must target a declared systemMode.
                    auto itn = g_nodes.find(current_node);
                    if (itn != g_nodes.end() && !itn->second.is_controller) {
                        diag.error(trans->loc, "Node '" + current_node + "' is not a Controller. It cannot perform System Transitions.");
                        has_error = true;
                    }
                    if (g_system_modes.find(trans->target_state) == g_system_modes.end()) {
                        diag.error(trans->loc, "Unknown system mode '" + trans->target_state + "'");
                        has_error = true;
                    }
                } else {
                    // Local / cross-node transitions.
                    std::string target_node = trans->target_node.empty() ? current_node : trans->target_node;

                    // Cross-node transitions are restricted to controllers.
                    if (!trans->target_node.empty()) {
                        auto itn = g_nodes.find(current_node);
                        if (itn != g_nodes.end() && !itn->second.is_controller) {
                            diag.error(trans->loc, "Node '" + current_node + "' is not a Controller. It cannot transition other nodes.");
                            has_error = true;
                        }
                    }

                    // Target node must exist.
                    if (g_nodes.find(target_node) == g_nodes.end()) {
                        diag.error(trans->loc, "Unknown target node '" + target_node + "' in transition");
                        has_error = true;
                        continue;
                    }

                    // Target state must be a known *local* mode for that node.
                    auto itset = g_local_modes_by_node.find(target_node);
                    bool ok = (itset != g_local_modes_by_node.end() &&
                               itset->second.find(trans->target_state) != itset->second.end());
                    if (!ok) {
                        if (g_system_modes.find(trans->target_state) != g_system_modes.end()) {
                            diag.error(trans->loc,
                                       "'" + trans->target_state + "' is a system mode. Use 'transition system \"" + trans->target_state + "\"' (or mark the mode 'ignore system')");
                        } else {
                            diag.error(trans->loc, "Unknown local mode '" + trans->target_state + "' for node '" + target_node + "'");
                        }
                        has_error = true;
                    }
                }
            }

            if (auto call = std::get_if<CallStmt>(&sp->v)) {
                if (g_nodes.find(current_node) == g_nodes.end()) continue;
                auto& ns = g_nodes[current_node];
                if (ns.private_funcs.find(call->callee) == ns.private_funcs.end()) {
                    diag.error(call->loc, "Unknown private function '" + call->callee + "'");
                    has_error = true;
                } else {
                    auto& func_sym = ns.private_funcs[call->callee];
                    if (call->args.size() != func_sym.param_types.size()) {
                        diag.error(call->loc, "Arg count mismatch in local call");
                        has_error = true;
                    }
                }
            }

            if (auto ifs = std::get_if<IfStmt>(&sp->v)) {
                ValType ct = infer_expr(infer_expr, ifs->cond, current_node, current_params);
                if (ct != ValType::Bool) {
                    diag.error(ifs->loc, "If condition must be bool");
                    has_error = true;
                }
                validate_stmts(ifs->then_body, current_node, current_params);
                for (const auto& br : ifs->elifs) {
                    ValType bt = infer_expr(infer_expr, br.cond, current_node, current_params);
                    if (bt != ValType::Bool) {
                        diag.error(br.loc, "Elif condition must be bool");
                        has_error = true;
                    }
                    validate_stmts(br.body, current_node, current_params);
                }
                validate_stmts(ifs->else_body, current_node, current_params);
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
            for (const auto& req : n->requests)       validate_stmts(req.body, n->name, req.sig.params);
            for (const auto& func : n->private_funcs) validate_stmts(func.body, n->name, func.sig.params);
            for (const auto& lis : n->listeners)      validate_listener(lis, n->name);
        } else if (auto m = std::get_if<ModeDecl>(&decl)) {
            validate_stmts(m->body, m->node_name, {});
            for (const auto& lis : m->listeners)      validate_listener(lis, m->node_name);
        }
    }

    return !has_error;
}


bool validate_program(const Program& program, const DiagnosticEngine& diag) {
    collect_symbols(program, diag);
    return check_logic(program, diag);
}
