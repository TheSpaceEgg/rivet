#include "validate.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>

static bool is_reserved_mode(const std::string& s) {
    return s == "boot" ||
           s == "normal" ||
           s == "idle" ||
           s == "fault" ||
           s == "shutdown";
}

bool validate_program(const Program& program, const DiagnosticEngine& diag) {
    bool ok = true;

    // -----------------------------
    // 1) Nodes: existence + duplicates
    // -----------------------------
    std::unordered_map<std::string, SourceLoc> node_locs;

    for (const auto& d : program.decls) {
        if (const auto* n = std::get_if<NodeDecl>(&d)) {
            if (node_locs.contains(n->name)) {
                ok = false;
                diag.error(n->loc, "Duplicate node declaration '" + n->name + "'");
            } else {
                node_locs.emplace(n->name, n->loc);
            }
        }
    }

    // -----------------------------
    // 2) Modes: unknown node + duplicate bindings
    //    Store: modes_by_node[node][mode_text] = ModeDecl*
    // -----------------------------
    std::unordered_map<std::string, std::unordered_map<std::string, const ModeDecl*>> modes_by_node;

    for (const auto& d : program.decls) {
        const auto* m = std::get_if<ModeDecl>(&d);
        if (!m) continue;

        // Unknown node referenced by a mode binding
        if (!node_locs.contains(m->node_name)) {
            ok = false;
            diag.error(m->loc, "Mode refers to unknown node '" + m->node_name + "'");
        }

        // Mode name string we key by (NOT ModeName struct!)
        const std::string& mode_text = m->mode_name.text;

        // Duplicate mode binding check per node
        auto& by_mode = modes_by_node[m->node_name];
        if (by_mode.contains(mode_text)) {
            ok = false;
            diag.error(m->loc, "Duplicate mode binding '" + m->node_name + "->" + mode_text + "'");
        } else {
            by_mode.emplace(mode_text, m);
        }

        // Delegation sanity: for now, only allow reserved targets
        if (m->delegate_to.has_value()) {
            const std::string& tgt = *m->delegate_to;
            if (!is_reserved_mode(tgt)) {
                ok = false;
                diag.error(m->loc, "Delegation target '" + tgt + "' is not a reserved mode");
            }
        }
    }

    // -----------------------------
    // 3) Circular delegation detection (per node)
    // -----------------------------
    enum class Mark { None, Visiting, Done };

    for (auto& [node, mode_map] : modes_by_node) {
        std::unordered_map<std::string, Mark> mark;

        auto dfs = [&](auto&& self, const std::string& mode) -> void {
            auto it = mode_map.find(mode);
            if (it == mode_map.end()) return;

            Mark& m = mark[mode];
            if (m == Mark::Visiting) {
                ok = false;
                diag.error(it->second->loc,
                           "Circular mode delegation detected for '" + node + "->" + mode + "'");
                return;
            }
            if (m == Mark::Done) return;

            m = Mark::Visiting;

            const ModeDecl* decl = it->second;
            if (decl->delegate_to.has_value()) {
                const std::string& tgt = *decl->delegate_to;

                // Only follow edges within this node (cycle detection is per-node)
                auto jt = mode_map.find(tgt);
                if (jt != mode_map.end()) {
                    self(self, tgt);
                }
            }

            m = Mark::Done;
        };

        for (const auto& [mode_name, _decl] : mode_map) {
            dfs(dfs, mode_name);
        }
    }

    return ok;
}
