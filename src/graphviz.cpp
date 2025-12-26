#include "graphviz.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <variant>

// ---------------------------------------------------------
// Helper Functions
// ---------------------------------------------------------

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

static std::string safe_id(const std::string& node, const std::string& handle) {
    return node + "__" + handle;
}

static void scan_stmts_for_edges(const std::vector<Stmt>& stmts, 
                                 const std::string& current_node_name, 
                                 std::ostream& os) {
    for (const auto& stmt : stmts) {
        if (std::holds_alternative<PublishStmt>(stmt)) {
            const auto& pub = std::get<PublishStmt>(stmt);
            std::string topic_id = safe_id(current_node_name, pub.topic_handle);
            os << "    " << current_node_name << " -> " << topic_id << " [color=blue];\n";
        }
        else if (std::holds_alternative<RequestStmt>(stmt)) {
            const auto& req = std::get<RequestStmt>(stmt);
            os << "    " << current_node_name << " -> " << req.target_node 
               << " [style=dashed, label=\"" << req.func_name << "\"];\n";
        }
    }
}

// ---------------------------------------------------------
// Core DOT Generation
// ---------------------------------------------------------

void generate_dot(const Program& p, std::ostream& os) {
    os << "digraph RivetArchitecture {\n";
    os << "  rankdir=LR;\n";
    os << "  node [fontname=\"Arial\", shape=box, style=filled, fillcolor=white];\n";
    os << "  edge [fontname=\"Arial\", fontsize=10];\n";
    os << "  graph [style=filled, fillcolor=\"#eeeeee\"];\n";

    // 1. Draw Nodes (Clusters)
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            os << "\n  subgraph cluster_" << n->name << " {\n";
            os << "    label = \"" << n->name << " : " << n->type_name << "\";\n";
            os << "    style = rounded;\n";
            os << "    color = black;\n";
            os << "    bgcolor = white;\n";
            
            os << "    " << n->name << " [label=\"" << n->name << "\", shape=component, fillcolor=\"#d0e0ff\"];\n";

            for (const auto& t : n->topics) {
                std::string tid = safe_id(n->name, t.name);
                os << "    " << tid << " [label=\"" << t.path << "\\n<" << type_str(t.type) << ">\", shape=ellipse, style=filled, fillcolor=\"#ddffdd\"];\n";
            }
            os << "  }\n";
        }
    }

    // 2. Draw Edges from Node Declarations
    for (const auto& decl : p.decls) {
        if (auto n = std::get_if<NodeDecl>(&decl)) {
            for (const auto& lis : n->listeners) {
                std::string target_node = lis.source_node.empty() ? n->name : lis.source_node;
                std::string tid = safe_id(target_node, lis.topic_name);
                os << "  " << tid << " -> " << n->name << " [color=green];\n";
                scan_stmts_for_edges(lis.body, n->name, os);
            }
            for (const auto& req : n->requests) {
                scan_stmts_for_edges(req.body, n->name, os);
            }
            for (const auto& func : n->private_funcs) {
                scan_stmts_for_edges(func.body, n->name, os);
            }
        }
        // 3. NEW: Draw Edges from Mode Declarations
        else if (auto m = std::get_if<ModeDecl>(&decl)) {
            // Scan statements inside mode blocks (like mode Power->Init)
            scan_stmts_for_edges(m->body, m->node_name, os);
        }
    }
    os << "}\n";
}

// ---------------------------------------------------------
// HTML Visualization
// ---------------------------------------------------------

void generate_and_open_html(const Program& p, const std::string& filename) {
    std::stringstream ss;
    generate_dot(p, ss);
    std::string dot_content = ss.str();

    std::ofstream html(filename);
    if (!html) {
        std::cerr << "Error: Could not write to " << filename << "\n";
        return;
    }

    html << R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Rivet System Architecture</title>
    <style>
        body { margin: 0; padding: 0; overflow: hidden; background-color: #f0f0f0; }
        #graph { width: 100vw; height: 100vh; display: flex; justify-content: center; align-items: center; }
        svg { width: 100%; height: 100%; }
    </style>
</head>
<body>
    <div id="graph">Loading Diagram...</div>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/viz.js/2.1.2/viz.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/viz.js/2.1.2/full.render.js"></script>
    <script>
        var dotLines = `)" << dot_content << R"(`;
        var viz = new Viz();
        viz.renderSVGElement(dotLines)
            .then(function(element) {
                var container = document.getElementById("graph");
                container.innerHTML = "";
                container.appendChild(element);
            })
            .catch(function(error) {
                console.error(error);
                document.getElementById("graph").innerHTML = 
                    "<h3 style='color:red'>Error rendering graph</h3><pre>" + error + "</pre>";
            });
    </script>
</body>
</html>
    )";

    html.close();
    std::string cmd = "start " + filename;
    system(cmd.c_str());
}