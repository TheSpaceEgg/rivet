#include "graphviz.hpp"
#include <iostream>
#include <unordered_map>

static std::string type_str(const TypeInfo& t) {
    switch(t.base) {
        case ValType::Int: return "int";
        case ValType::Float: return "float";
        case ValType::String: return "string";
        case ValType::Bool: return "bool";
        case ValType::Custom: return t.custom_name;
    }
    return "?";
}

// Helper to sanitize names for Graphviz IDs
static std::string safe_id(const std::string& node, const std::string& handle) {
    return node + "__" + handle;
}

// Recursive helper to find "publish" and "request" inside statements
static void scan_stmts_for_edges(const std::vector<Stmt>& stmts, 
                                 const std::string& current_node_name, 
                                 std::ostream& os) {
    for (const auto& stmt : stmts) {
        if (std::holds_alternative<PublishStmt>(stmt)) {
            const auto& pub = std::get<PublishStmt>(stmt);
            // Edge: Node -> Topic
            std::string topic_id = safe_id(current_node_name, pub.topic_handle);
            os << "    " << current_node_name << " -> " << topic_id << " [color=blue];\n";
        }
        else if (std::holds_alternative<RequestStmt>(stmt)) {
            const auto& req = std::get<RequestStmt>(stmt);
            // Edge: Node -> TargetNode (dashed for request)
            os << "    " << current_node_name << " -> " << req.target_node 
               << " [style=dashed, label=\"" << req.func_name << "\"];\n";
        }
    }
}

void generate_dot(const Program& p, std::ostream& os) {
    os << "digraph RivetArchitecture {\n";
    os << "  rankdir=LR;\n"; // Left-to-Right layout
    os << "  node [fontname=\"Arial\", shape=box, style=filled, fillcolor=white];\n";
    os << "  edge [fontname=\"Arial\", fontsize=10];\n";
    os << "  graph [style=filled, fillcolor=\"#eeeeee\"];\n";

    // 1. Draw Nodes and their Topics (Clusters)
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            os << "\n  subgraph cluster_" << n->name << " {\n";
            os << "    label = \"" << n->name << " : " << n->type_name << "\";\n";
            os << "    style = rounded;\n";
            os << "    color = black;\n";
            os << "    bgcolor = white;\n";
            
            // Define the Node as an invisible point for incoming/outgoing arrows
            // OR simply treat the cluster as the entity. 
            // Better strategy: Create a main node block for the logic
            os << "    " << n->name << " [label=\"" << n->name << "\", shape=component, fillcolor=\"#d0e0ff\"];\n";

            // Define Topics owned by this node
            for (const auto& t : n->topics) {
                std::string tid = safe_id(n->name, t.name);
                os << "    " << tid << " [label=\"" << t.path << "\\n<" << type_str(t.type) << ">\", shape=ellipse, style=filled, fillcolor=\"#ddffdd\"];\n";
            }
            os << "  }\n";
        }
    }

    // 2. Draw Edges (Publish / Subscribe / Request)
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            
            // A. LISTENERS (Subscribe): Topic -> Node
            for (const auto& lis : n->listeners) {
                std::string target_node = lis.source_node.empty() ? n->name : lis.source_node;
                std::string tid = safe_id(target_node, lis.topic_name);
                
                // Edge: Topic -> This Node
                os << "  " << tid << " -> " << n->name << " [color=green];\n";
                
                // Scan body for outgoing requests/publishes
                scan_stmts_for_edges(lis.body, n->name, os);
            }

            // B. REQUEST HANDLERS: Scan bodies for outbound logic
            for (const auto& req : n->requests) {
                scan_stmts_for_edges(req.body, n->name, os);
            }

            // C. PRIVATE FUNCTIONS: Scan bodies
            for (const auto& func : n->private_funcs) {
                scan_stmts_for_edges(func.body, n->name, os);
            }
        }
    }

    os << "}\n";
}