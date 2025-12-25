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

    // 1) nodes + duplicates
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

    // 2) global/system modes
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

    // 3) gather per-node mode definitions and local string modes
    // modes_by_node[node][mode_text] = ModeDecl*
    std::unordered_map<std::string, std::unordered_map<std::string, const ModeDecl*>> modes_by_node;
    // local_modes[node] = set of local string modes
    std::unordered_map<std::string, std::unordered_set<std::string>> local_modes;

    for (const auto& d : program.decls) {
        const auto* m = std::get_if<ModeDecl>(&d);
        if (!m) continue;

        if (!map_has(node_locs, m->node_name)) {
            ok = false;
            diag.error(m->loc, "Mode refers to unknown node '" + m->node_name + "'");
        }

        const std::string& mode_text = m->mode_name.text;

        // Enforce rule:
        // - identifier mode names must be reserved or declared systemmode
        // - node-local modes must be quoted strings
        if (!m->mode_name.is_local_string) {
            if (!is_reserved_mode(mode_text) && !set_has(system_modes, mode_text)) {
                ok = false;
                diag.error(m->mode_name.loc,
                           "Unknown mode '" + mode_text + "'. Declare it with 'systemmode " + mode_text +
                           "' or use a node-local string mode: \"" + mode_text + "\"");
            }
        } else {
            local_modes[m->node_name].insert(mode_text);
        }

        // duplicates per node+mode text
        auto& by_mode = modes_by_node[m->node_name];
        if (by_mode.find(mode_text) != by_mode.end()) {
            ok = false;
            diag.error(m->loc, "Duplicate mode binding '" + m->node_name + "->" + mode_text + "'");
        } else {
            by_mode.emplace(mode_text, m);
        }
    }

    // 4) validate delegation targets + build for cycle checks
    // Cycle checks are per-node through explicit do-chains.
    enum class Mark { None, Visiting, Done };

    for (auto& node_pair : modes_by_node) {
        const std::string& node = node_pair.first;
        auto& mode_map = node_pair.second;

        // Validate each delegation target (resolve identifier to local string if needed)
        for (auto& it : mode_map) {
            const std::string& mode_name = it.first;
            const ModeDecl* decl = it.second;

            if (!decl->delegate_to.has_value()) continue;

            const ModeName& tgt = *decl->delegate_to;

            // Resolve target name for this node:
            // - if quoted, it’s local and must exist for this node
            // - if ident:
            //     - if reserved/system: ok (may or may not be defined for this node)
            //     - else: allow if a local string mode of same text exists for this node (your desired behaviour)
            bool target_exists_as_local = false;
            auto lm_it = local_modes.find(node);
            if (lm_it != local_modes.end()) {
                target_exists_as_local = lm_it->second.find(tgt.text) != lm_it->second.end();
            }

            if (tgt.is_local_string) {
                if (!target_exists_as_local) {
                    ok = false;
                    diag.error(tgt.loc, "Delegation target \"" + tgt.text + "\" is not a known node-local mode for '" + node + "'");
                }
            } else {
                if (!is_reserved_mode(tgt.text) && !set_has(system_modes, tgt.text) && !target_exists_as_local) {
                    ok = false;
                    diag.error(tgt.loc, "Delegation target '" + tgt.text + "' is not reserved, not a system mode, and not a node-local mode for '" + node + "'");
                }
            }
        }

        // Cycle detection (follow only edges that point to a mode *defined on this node*)
        std::unordered_map<std::string, Mark> mark;

        auto resolve_for_cycle = [&](const ModeDecl* decl) -> std::string {
            // if target is ident but matches local string mode, treat as that name
            const ModeName& tgt = *decl->delegate_to;
            if (!tgt.is_local_string) {
                auto lm_it = local_modes.find(node);
                if (lm_it != local_modes.end() && lm_it->second.find(tgt.text) != lm_it->second.end()) {
                    return tgt.text; // local mode name
                }
            }
            return tgt.text;
        };

        auto dfs = [&](auto&& self, const std::string& mode) -> void {
            auto it = mode_map.find(mode);
            if (it == mode_map.end()) return;

            Mark& mk = mark[mode];
            if (mk == Mark::Visiting) {
                ok = false;
                diag.error(it->second->loc, "Circular mode delegation detected for '" + node + "->" + mode + "'");
                return;
            }
            if (mk == Mark::Done) return;

            mk = Mark::Visiting;

            const ModeDecl* decl = it->second;
            if (decl->delegate_to.has_value()) {
                std::string tgt_resolved = resolve_for_cycle(decl);
                if (mode_map.find(tgt_resolved) != mode_map.end()) {
                    self(self, tgt_resolved);
                }
            }

            mk = Mark::Done;
        };

        for (auto& it : mode_map) {
            dfs(dfs, it.first);
        }
    }

    return ok;
}
