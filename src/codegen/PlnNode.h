/// Palan AST node definitions for code generation.
///
/// @file PlnNode.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <vector>
#include <memory>

using std::string;
using std::vector;
using std::unique_ptr;

// -------- Expressions --------

struct Expr {
    virtual ~Expr() = default;
};

struct StrLitExpr : Expr {
    string value;
};

struct IntLitExpr : Expr {
    string value;  // decimal string
};

struct UintLitExpr : Expr {
    string value;  // decimal string
};

struct IdExpr : Expr {
    string name;
};

struct CCCallExpr : Expr {
    string name;
    vector<unique_ptr<Expr>> args;
};

struct PlnCallExpr : Expr {
    string name;
    vector<unique_ptr<Expr>> args;
};

// -------- Statements --------

struct Stmt {
    virtual ~Stmt() = default;
};

struct ExprStmt : Stmt {
    unique_ptr<Expr> body;
};

struct VarEntry {
    string           varName;
    string           typeName;   // "int64" etc.
    unique_ptr<Expr> init;       // nullable
};

struct VarDeclStmt : Stmt {
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
