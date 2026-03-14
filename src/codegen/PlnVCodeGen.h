/// VCodeGen: lowers C++ AST nodes to virtual instruction set (VProg).
///
/// @file PlnVCodeGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <map>
#include <string>
#include <utility>
#include "PlnNode.h"
#include "PlnVProg.h"

using std::map;
using std::pair;
using std::string;

class PlnVCodeGen {
    map<string, string> strLiterals;  // value -> label
    int nextVReg = 0;
    int stackOffset = 0;                           // current stack offset (negative)
    map<string, pair<VRegType, int>> localVarMap;  // var name -> {type, offset}

    VReg allocVReg();

    void lowerStmt(const Stmt& stmt, VFunc& func);
    void lowerExprStmt(const ExprStmt& stmt, VFunc& func);
    void lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func);
    void lowerCCCallExpr(const CCCallExpr& expr, VFunc& func);
    void lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func);

public:
    VProg generate(const Module& module);
};
