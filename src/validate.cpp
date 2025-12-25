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

static bool map_has(const std::unordered_map<std::string, SourceLoc>& m, const std::string& k) {
    return m.find(k) != m.end();
}

static bool set_has(const std::unordered_set<std::string>& s, const std::string& k) {
    return s.find(k) != s.end();
}

bool validate_program(const Program& program, const DiagnosticEngine& diag) {
    bool ok = true;

    // ------------------------------------------------------------
    // 1) Collect nodes + detect duplicates
    // ------------------------------------------------------------
    std::unordered_map<std::string, SourceLoc> node_locs;
    for (const auto& d : program.decls) {
        if (const auto* n = std::get_if<NodeDecl>(&d)) {
            if (map_has(node_locs, n->name)) {
                ok = false;
                diag.error(n->loc, "Duplicate node declaration '" + n->name + "'");
            } else {
                node_locs.emplace(n->name, n->loc);
            }
        }
    }

    // ------------------------------------------------------------
    // 2) Collect system/global modes (systemmode <ident>)
    // ------------------------------------------------------------
    std::unordered_set<std::string> system_modes;
    for (const auto& d : program.decls) {
        if (const auto* sm = std::get_if<SystemModeDecl>(&d)) {
            if (is_reserved_mode(sm->name)) {
                ok = false;
                diag.error(sm->loc, "System mode '" + sm->name + "' is reserved and does not need declaring");
            } else {
                system_modes.insert(sm->name);
            }
        }
    }

    // ------------------------------------------------------------
    // 3) Gather mode definitions:
    //    modes_by_node[node][mode_text] = ModeDecl*
    //    local_modes[node] = { "calibrate", ... } for quoted modes
    // ------------------------------------------------------------
    std::unordered_map<std::string, std::unordered_map<std::string, const ModeDecl*>> modes_by_node;
    std::unordered_map<std::string, std::unordered_set<std::string>> local_modes;

    for (const auto& d : program.decls) {
        const auto* m = std::get_if<ModeDecl>(&d);
        if (!m) continue;

        if (!map_has(node_locs, m->node_name)) {
            ok = false;
            diag.error(m->loc, "Mode refers to unknown node '" + m->node_name + "'");
        }

        const std::string& mode_text = m->mode_name.text;

        // Identifier modes must be reserved or declared systemmode.
        // String modes are node-local by definition.
        if (!m->mode_name.is_local_string) {
            if (!is_reserved_mode(mode_text) && !set_has(system_modes, mode_text)) {
                ok = false;
                diag.error(
                    m->mode_name.loc,
                    "Unknown mode '" + mode_text +
                    "'. Declare it with 'systemmode " + mode_text +
                    "' or use a node-local string mode: \"" + mode_text + "\""
                );
            }
        } else {
            local_modes[m->node_name].insert(mode_text);
        }

        // Duplicate binding check per node+mode_text
        auto& by_mode = modes_by_node[m->node_name];
        if (by_mode.find(mode_text) != by_mode.end()) {
            ok = false;
            diag.error(m->loc, "Duplicate mode binding '" + m->node_name + "->" + mode_text + "'");
        } else {
            by_mode.emplace(mode_text, m);
        }
    }

    // ------------------------------------------------------------
    // 4) Validate delegation targets + cycle detection (per node)
    // ------------------------------------------------------------
    enum class Mark { None, Visiting, Done };

    for (auto node_it = modes_by_node.begin(); node_it != modes_by_node.end(); ++node_it) {
        const std::string& node = node_it->first;
        auto& mode_map = node_it->second;

        auto lm_it = local_modes.find(node);
        const bool has_local = (lm_it != local_modes.end());

        auto is_local_for_node = [&](const std::string& s) -> bool {
            if (!has_local) return false;
            return lm_it->second.find(s) != lm_it->second.end();
        };

        // ---- 4a) Validate each do-target resolves
        for (auto it = mode_map.begin(); it != mode_map.end(); ++it) {
            const ModeDecl* decl = it->second;
            if (!decl->delegate_to.has_value()) continue;

            const ModeName& tgt = *decl->delegate_to;
            const std::string& tgt_text = tgt.text;

            // If target is quoted, it must exist as a local mode.
            if (tgt.is_local_string) {
                if (!is_local_for_node(tgt_text)) {
                    ok = false;
                    diag.error(tgt.loc,
                               "Delegation target \"" + tgt_text +
                               "\" is not a known node-local mode for '" + node + "'");
                }
                continue;
            }

            // If target is an identifier, accept if:
            // - reserved
            // - systemmode
            // - OR a node-local mode with the same text exists (your desired behaviour)
            if (!is_reserved_mode(tgt_text) &&
                !set_has(system_modes, tgt_text) &&
                !is_local_for_node(tgt_text)) {
                ok = false;
                diag.error(tgt.loc,
                           "Delegation target '" + tgt_text +
                           "' is not reserved, not a system mode, and not a node-local mode for '" + node + "'");
            }
        }

        // ---- 4b) Cycle detection (follow edges only to modes defined on this node)
        std::unordered_map<std::string, Mark> mark;

        auto resolve_target_for_cycle = [&](const ModeDecl* decl) -> std::string {
            const ModeName& tgt = *decl->delegate_to;
            // If ident matches a local string mode name, treat it as that local mode.
            if (!tgt.is_local_string && is_local_for_node(tgt.text)) {
                return tgt.text;
            }
            return tgt.text;
        };

        auto dfs = [&](auto&& self, const std::string& mode_name) -> void {
            auto it = mode_map.find(mode_name);
            if (it == mode_map.end()) return;

            Mark& mk = mark[mode_name];
            if (mk == Mark::Visiting) {
                ok = false;
                diag.error(it->second->loc,
                           "Circular mode delegation detected for '" + node + "->" + mode_name + "'");
                return;
            }
            if (mk == Mark::Done) return;

            mk = Mark::Visiting;

            const ModeDecl* decl = it->second;
            if (decl->delegate_to.has_value()) {
                std::string tgt = resolve_target_for_cycle(decl);
                if (mode_map.find(tgt) != mode_map.end()) {
                    self(self, tgt);
                }
            }

            mk = Mark::Done;
        };

        for (auto it = mode_map.begin(); it != mode_map.end(); ++it) {
            dfs(dfs, it->first);
        }
    }

    return ok;
}
