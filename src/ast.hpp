#pragma once
#include "source.hpp"
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

struct TopicDecl {
    SourceLoc loc{};
    std::string name; 
    std::string path; 
    TypeInfo type;
};

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

struct PublishStmt {
    SourceLoc loc{};
    std::string topic_handle; 
    std::string value; 
};

struct ReturnStmt {
    SourceLoc loc{};
    std::string value;
};

struct TransitionStmt {
    SourceLoc loc{};
    bool is_system = false;     
    std::string target_state;
};

// NEW: Log Statement
enum class LogLevel { Print, Info, Warn, Error, Debug };

struct LogStmt {
    SourceLoc loc{};
    LogLevel level = LogLevel::Info;
    std::vector<std::string> args;
};

using Stmt = std::variant<CallStmt, RequestStmt, PublishStmt, ReturnStmt, TransitionStmt, LogStmt>;

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
    std::string source_node; 
    std::string topic_name;  
    std::string delegate_to; 
    FuncSignature sig;
    std::vector<Stmt> body;
};

struct NodeDecl {
    SourceLoc loc{};
    bool is_controller = false;    
    bool ignores_system = false;   
    std::string name;
    std::string type_name;
    std::string config_text;
    std::vector<TopicDecl> topics;        
    std::vector<OnRequestDecl> requests;  
    std::vector<OnListenDecl> listeners; 
    std::vector<FuncDecl> private_funcs; 
};

struct ModeName {
    SourceLoc loc{};
    bool is_local_string = false;
    std::string text;
};

struct ModeDecl {
    SourceLoc loc{};
    bool ignores_system = false;   
    std::string node_name;
    ModeName mode_name;
    std::vector<Stmt> body;
    std::vector<OnListenDecl> listeners;
};

using Decl = std::variant<SystemModeDecl, NodeDecl, ModeDecl, FuncDecl>;

struct Program {
    std::vector<Decl> decls;
};