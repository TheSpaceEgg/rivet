#pragma once
#include "source.hpp"
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct SystemModeDecl {
    SourceLoc loc{};
    std::string name; // identifier name (no quotes)
};

struct NodeDecl {
    SourceLoc loc{};
    std::string name;
    std::string type_name;
    std::string config_text; // raw "{...}" for now
};

struct CallStmt {
    SourceLoc loc{};
    std::string callee;
    std::vector<std::string> args; // raw args for now
};

using Stmt = std::variant<CallStmt>;

struct ModeName {
    // Exactly one of these is used:
    bool is_local_string = false;  // true if written as "..."
    std::string text;             // either identifier text or string contents (no quotes)
    SourceLoc loc{};
};

struct ModeDecl {
    SourceLoc loc{};
    std::string node_name;

    ModeName mode_name;

    // Either body OR delegation
    std::vector<Stmt> body;
    std::optional<std::string> delegate_to; // target mode (identifier only, for now)
};

using Decl = std::variant<SystemModeDecl, NodeDecl, ModeDecl>;

struct Program {
    std::vector<Decl> decls;
};
