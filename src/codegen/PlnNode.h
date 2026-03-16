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
    string value;  // decimal string
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
    vector<unique_ptr<Expr>> args;
};

struct PlnCallExpr : Expr {
    PlnCallExpr() : Expr(ExprKind::PlnCall) {}
    string name;
    vector<unique_ptr<Expr>> args;
};

// -------- Statements --------

enum class StmtKind {
    Expr,
    VarDecl,
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

// -------- Module --------

struct StrLiteralDef {
    string label;
    string value;
};

struct Module {
    string original;
    vector<StrLiteralDef> strLiterals;
    vector<unique_ptr<Stmt>> statements;
};
