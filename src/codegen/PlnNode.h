/// Palan AST node definitions for code generation.
///
/// @file PlnNode.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "PlnVProg.h"

using std::string;
using std::vector;
using std::unique_ptr;

// -------- Expressions --------

enum class ExprKind {
    StrLit,
    IntLit,
    UintLit,
    Id,
    Add,
    Sub,
    Cmp,
    Convert,
    CCCall,
    PlnCall,
};

struct Expr {
    const ExprKind kind;
    virtual ~Expr() = default;
protected:
    explicit Expr(ExprKind k) : kind(k) {}
};

struct StrLitExpr : Expr {
    StrLitExpr() : Expr(ExprKind::StrLit) {}
    string label;
};

struct IntLitExpr : Expr {
    IntLitExpr() : Expr(ExprKind::IntLit) {}
    string   value;  // decimal string
    VRegType type = VRegType::Int64;
};

struct UintLitExpr : Expr {
    UintLitExpr() : Expr(ExprKind::UintLit) {}
    string value;  // decimal string
};

struct IdExpr : Expr {
    IdExpr() : Expr(ExprKind::Id) {}
    string name;
};

struct AddExpr : Expr {
    AddExpr() : Expr(ExprKind::Add) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct SubExpr : Expr {
    SubExpr() : Expr(ExprKind::Sub) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct CmpExpr : Expr {
    CmpExpr() : Expr(ExprKind::Cmp) {}
    string           op;           // "<", "<=", ">", ">=", "==", "!="
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType operandType = VRegType::Int64;  // type of lhs/rhs operands
};

struct ConvertExpr : Expr {
    ConvertExpr() : Expr(ExprKind::Convert) {}
    VRegType         from;
    VRegType         to;
    unique_ptr<Expr> src;
};

struct CCCallExpr : Expr {
    CCCallExpr() : Expr(ExprKind::CCCall) {}
    string name;
    bool     hasRet = false;
    VRegType retType = VRegType::Int64;
    vector<unique_ptr<Expr>> args;
};

struct PlnCallExpr : Expr {
    PlnCallExpr() : Expr(ExprKind::PlnCall) {}
    string name;
    bool     hasRet  = false;
    VRegType retType = VRegType::Int64;
    vector<unique_ptr<Expr>> args;
};

struct VarDef { string name; VRegType type; };

// -------- Statements --------

enum class StmtKind {
    Expr,
    VarDecl,
    Assign,
    Return,
    TappleDecl,
    Block,
    If,
};

struct Stmt {
    const StmtKind kind;
    virtual ~Stmt() = default;
protected:
    explicit Stmt(StmtKind k) : kind(k) {}
};

struct ExprStmt : Stmt {
    ExprStmt() : Stmt(StmtKind::Expr) {}
    unique_ptr<Expr> body;
};

struct VarEntry {
    string           varName;
    string           typeName;   // "int64" etc.
    unique_ptr<Expr> init;       // nullable
};

struct VarDeclStmt : Stmt {
    VarDeclStmt() : Stmt(StmtKind::VarDecl) {}
    vector<VarEntry> vars;
};

struct AssignStmt : Stmt {
    AssignStmt() : Stmt(StmtKind::Assign) {}
    string           name;   // destination variable name
    unique_ptr<Expr> value;
};

struct ReturnStmt : Stmt {
    ReturnStmt() : Stmt(StmtKind::Return) {}
    vector<unique_ptr<Expr>> values;  // empty for bare return
};

struct TappleDeclStmt : Stmt {
    TappleDeclStmt() : Stmt(StmtKind::TappleDecl) {}
    vector<VarDef>           vars;
    string                   funcName;
    vector<unique_ptr<Expr>> args;
    vector<VRegType>         retTypes;
};

struct BlockStmt : Stmt {
    BlockStmt() : Stmt(StmtKind::Block) {}
    vector<unique_ptr<Stmt>> body;
};

struct IfStmt : Stmt {
    IfStmt() : Stmt(StmtKind::If) {}
    unique_ptr<Expr> cond;
    unique_ptr<Stmt> thenStmt;  // always BlockStmt
    unique_ptr<Stmt> elseStmt;  // nullptr | BlockStmt | IfStmt (else-if chain)
};

// -------- Module --------

struct StrLiteralDef {
    string label;
    string value;
};

struct PlnFunc {
    string             name;
    vector<VarDef>     params;
    bool               hasRetType = false;
    VRegType           retType    = VRegType::Int64;
    string             retVarName;   // single named return variable; empty if none
    vector<VarDef>     retVars;      // multiple named return variables (size>=2)
    vector<unique_ptr<Stmt>> body;
    bool               isExport = false;
};

struct Module {
    string original;
    vector<StrLiteralDef>    strLiterals;
    vector<PlnFunc>          functions;
    vector<unique_ptr<Stmt>> statements;
};
