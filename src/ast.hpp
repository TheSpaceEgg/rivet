#pragma once
#include "source.hpp"
#include <optional>
#include <string>
#include <variant>
#include <vector>

// --- Basic Types ---
enum class ValType { Int, Float, String, Bool, Custom };

struct TypeInfo {
    ValType base = ValType::Int;
    std::string custom_name; 
};

struct Param {
    SourceLoc loc{};
    std::string name;
    TypeInfo type;
};

struct SystemModeDecl {
    SourceLoc loc{};
    std::string name;
};

// --- Topic Declaration ---
struct TopicDecl {
    SourceLoc loc{};
    std::string name; // handle (e.g. "data")
    std::string path; // path (e.g. "imu/accel")
    TypeInfo type;
};

// --- Statements ---
struct CallStmt {
    SourceLoc loc{};
    std::string callee;
    std::vector<std::string> args; 
};

struct RequestStmt {
    SourceLoc loc{};
    bool is_silent = false;
    std::string target_node;
    std::string func_name;
    std::vector<std::string> args;
};

// NEW: Publish via handle
struct PublishStmt {
    SourceLoc loc{};
    std::string topic_handle; 
    std::string value; 
};

struct ReturnStmt {
    SourceLoc loc{};
    std::string value;
};

using Stmt = std::variant<CallStmt, RequestStmt, PublishStmt, ReturnStmt>;

// --- Function Declarations ---
struct FuncSignature {
    SourceLoc loc{};
    std::string name;
    std::vector<Param> params;
    TypeInfo return_type; 
};

struct FuncDecl {
    FuncSignature sig;
    std::vector<Stmt> body;
};

struct OnRequestDecl {
    FuncSignature sig;
    std::vector<Stmt> body;
    std::string delegate_to; 
};

struct OnListenDecl {
    SourceLoc loc{};
    std::string source_node; // e.g. "imu" (empty if local)
    std::string topic_name;  // e.g. "data"
    
    FuncSignature sig;
    std::vector<Stmt> body;
    std::string delegate_to; 
};

// --- Node Declarations ---
struct NodeDecl {
    SourceLoc loc{};
    std::string name;
    std::string type_name;
    std::string config_text;

    std::vector<TopicDecl> topics;        // Topics owned by this node
    std::vector<OnRequestDecl> requests;  
    std::vector<OnListenDecl> listeners; 
    std::vector<FuncDecl> private_funcs; 
};

// --- Mode Declarations ---
struct ModeName {
    SourceLoc loc{};
    bool is_local_string = false;
    std::string text;
};

struct ModeDecl {
    SourceLoc loc{};
    std::string node_name;
    ModeName mode_name;
    std::vector<Stmt> body;
    std::optional<ModeName> delegate_to;
};

// Note: TopicDecl is NOT here because it is inside NodeDecl now
using Decl = std::variant<SystemModeDecl, NodeDecl, ModeDecl, FuncDecl>;

struct Program {
    std::vector<Decl> decls;
};