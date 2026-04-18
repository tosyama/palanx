/// VCodeGen: lowers C++ AST nodes to virtual instruction set (VProg).
///
/// @file PlnVCodeGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <map>
#include <string>
#include <vector>
#include "PlnNode.h"
#include "PlnVProg.h"

using std::map;
using std::string;
using std::vector;

class PlnVCodeGen {
    int nextVReg = 0;
    int labelCounter_ = 0;       // unique label index for control-flow labels
    int floatLabelCounter_ = 0;  // unique label index for float literals
    VProg* prog_ = nullptr;      // set during generate(); used to push float literals

    // Variable symbol table: scoped stack of name -> VReg mappings.
    // enterVarScope/leaveVarScope push/pop; findVar searches from innermost.
    vector<map<string, VReg>> varScopes;
    void enterVarScope();
    void leaveVarScope();
    VReg findVar(const string& name) const;
    void declareVar(const string& name, VReg r);

    VReg allocVReg();

    const PlnFunc* currentPlnFunc_ = nullptr;  // set while lowering a Palan function body
    vector<vector<VReg>> blockVarStack_;

    struct LoopLabels { string start; string end; };
    vector<LoopLabels> loopStack_;  // push/pop in lowerWhileStmt

    string addFloatLiteral(const string& value, VRegType type);
    VReg lowerExpr(const Expr& expr, VFunc& func);
    void lowerStmt(const Stmt& stmt, VFunc& func);
    void lowerExprStmt(const ExprStmt& stmt, VFunc& func);
    void lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func);
    void lowerAssignStmt(const AssignStmt& stmt, VFunc& func);
    VReg computeArrAddr(VReg base, VReg idx, int scale, VFunc& func);
    VReg lowerArrIndexExpr(const ArrIndexExpr& expr, VFunc& func);
    void lowerArrAssignStmt(const ArrAssignStmt& stmt, VFunc& func);
    void lowerReturnStmt(const ReturnStmt& stmt, VFunc& func);
    void lowerCCCallExpr(const CCCallExpr& expr, VFunc& func);
    void lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func);
    void lowerTappleDeclStmt(const TappleDeclStmt& stmt, VFunc& func);
    void lowerBlockStmt(const BlockStmt& stmt, VFunc& func);
    void lowerIfStmt(const IfStmt& stmt, VFunc& func);
    void lowerWhileStmt(const WhileStmt& stmt, VFunc& func);
    void lowerBreakStmt(VFunc& func);
    void lowerContinueStmt(VFunc& func);

public:
    VProg generate(const Module& module, bool noEntry = false);
};
