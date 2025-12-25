#pragma once
#include "source.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

struct SystemModeDecl {
    SourceLoc loc{};
    std::string name; // identifier
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
    std::vector<std::string> args;
};

using Stmt = std::variant<CallStmt>;

struct ModeName {
    SourceLoc loc{};
    bool is_local_string = false; // true if written as "..."
    std::string text;             // identifier text OR string contents without quotes
};

struct ModeDecl {
    SourceLoc loc{};
    std::string node_name;
    ModeName mode_name;

    std::vector<Stmt> body;

    // Delegation target can be IDENT or STRING now
    std::optional<ModeName> delegate_to;
};

using Decl = std::variant<SystemModeDecl, NodeDecl, ModeDecl>;

struct Program {
    std::vector<Decl> decls;
};
