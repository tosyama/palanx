#include "PlnVCodeGen.h"
#include <boost/assert.hpp>

using namespace std;

// -------- Variable scope management --------

void PlnVCodeGen::enterVarScope()
{
    varScopes.push_back({});
}

void PlnVCodeGen::leaveVarScope()
{
    varScopes.pop_back();
}

VReg PlnVCodeGen::findVar(const string& name) const
{
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return f->second;
    }
    BOOST_ASSERT(false);  // variable not declared
    return -1;
}

void PlnVCodeGen::declareVar(const string& name, VReg r)
{
    varScopes.back()[name] = r;
}

// -------- Utilities --------

VReg PlnVCodeGen::allocVReg()
{
    return nextVReg++;
}

// -------- Lower AST to VInstr --------

void PlnVCodeGen::lowerCCCallExpr(const CCCallExpr& expr, VFunc& func)
{
    vector<VReg> args;
    for (auto& arg : expr.args) {
        if (auto* e = dynamic_cast<const StrLitExpr*>(arg.get())) {
            VReg r = allocVReg();
            func.instrs.push_back(LeaLabel{r, VRegType::Ptr64, strLiterals.at(e->value)});
            args.push_back(r);
        } else if (auto* e = dynamic_cast<const IntLitExpr*>(arg.get())) {
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, VRegType::Int64, stoll(e->value)});
            args.push_back(r);
        } else if (auto* e = dynamic_cast<const UintLitExpr*>(arg.get())) {
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, VRegType::Int64, (long long)stoull(e->value)});
            args.push_back(r);
        } else if (auto* e = dynamic_cast<const IdExpr*>(arg.get())) {
            // Use the variable's VReg directly; RegAlloc decides its location.
            args.push_back(findVar(e->name));
        } else {
            BOOST_ASSERT(false);  // not-impl
        }
    }
    func.instrs.push_back(CallC{expr.name, move(args)});
}

void PlnVCodeGen::lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func)
{
    BOOST_ASSERT(false);  // not-impl
}

void PlnVCodeGen::lowerExprStmt(const ExprStmt& stmt, VFunc& func)
{
    if (auto* e = dynamic_cast<const CCCallExpr*>(stmt.body.get())) {
        lowerCCCallExpr(*e, func);
        return;
    }
    if (auto* e = dynamic_cast<const PlnCallExpr*>(stmt.body.get())) {
        lowerPlnCallExpr(*e, func);
        return;
    }
    // other expression statements: not-impl
}

void PlnVCodeGen::lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func)
{
    for (auto& ve : stmt.vars) {
        VReg r = allocVReg();
        declareVar(ve.varName, r);
        if (ve.init) {
            if (auto* e = dynamic_cast<const IntLitExpr*>(ve.init.get())) {
                func.instrs.push_back(InitVar{r, VRegType::Int64, stoll(e->value)});
            } else {
                BOOST_ASSERT(false);  // other init exprs: 0204+
            }
        }
    }
}

void PlnVCodeGen::lowerStmt(const Stmt& stmt, VFunc& func)
{
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
        lowerExprStmt(*s, func);
        return;
    }
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        lowerVarDeclStmt(*s, func);
        return;
    }
    BOOST_ASSERT(false);
}

// -------- Entry point --------

VProg PlnVCodeGen::generate(const Module& module)
{
    // Build lookup map from SA-collected literals
    for (auto& d : module.strLiterals) {
        strLiterals[d.value] = d.label;
    }

    VProg prog;

    // Populate .rodata entries
    for (auto& d : module.strLiterals) {
        prog.data.push_back(VStringLiteral{d.label, d.value});
    }

    // Build _start function
    VFunc startFunc;
    startFunc.name = "_start";
    enterVarScope();
    for (auto& stmt : module.statements) {
        lowerStmt(*stmt, startFunc);
    }
    leaveVarScope();
    startFunc.instrs.push_back(ExitCode{0});
    prog.funcs.push_back(move(startFunc));

    return prog;
}
