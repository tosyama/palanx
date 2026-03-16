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

VReg PlnVCodeGen::lowerExpr(const Expr& expr, VFunc& func)
{
    switch (expr.kind) {
        case ExprKind::StrLit: {
            auto& e = static_cast<const StrLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(LeaLabel{r, VRegType::Ptr64, e.label});
            return r;
        }
        case ExprKind::IntLit: {
            auto& e = static_cast<const IntLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, VRegType::Int64, stoll(e.value)});
            return r;
        }
        case ExprKind::UintLit: {
            auto& e = static_cast<const UintLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, VRegType::Int64, (long long)stoull(e.value)});
            return r;
        }
        case ExprKind::Convert: {
            auto& e = static_cast<const ConvertExpr&>(expr);
            VReg src = lowerExpr(*e.src, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Convert{dst, src, e.from, e.to});
            return dst;
        }
        case ExprKind::Id: {
            auto& e = static_cast<const IdExpr&>(expr);
            return findVar(e.name);
        }
        case ExprKind::Add: {
            auto& e  = static_cast<const AddExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Add{dst, l, r, VRegType::Int64});
            return dst;
        }
        default:
            // CCCall/PlnCall do not produce a value; use lowerExprStmt instead.
            BOOST_ASSERT(false);
            return -1;
    }
}

void PlnVCodeGen::lowerCCCallExpr(const CCCallExpr& expr, VFunc& func)
{
    vector<VReg> args;
    for (auto& arg : expr.args) {
        args.push_back(lowerExpr(*arg, func));
    }
    func.instrs.push_back(CallC{expr.name, move(args)});
}

void PlnVCodeGen::lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func)
{
    BOOST_ASSERT(false);  // not-impl
}

void PlnVCodeGen::lowerExprStmt(const ExprStmt& stmt, VFunc& func)
{
    switch (stmt.body->kind) {
        case ExprKind::CCCall:
            lowerCCCallExpr(static_cast<const CCCallExpr&>(*stmt.body), func);
            return;
        case ExprKind::PlnCall:
            lowerPlnCallExpr(static_cast<const PlnCallExpr&>(*stmt.body), func);
            return;
        default:
            BOOST_ASSERT(false);  // only call expressions are valid as statements
    }
}

void PlnVCodeGen::lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func)
{
    for (auto& ve : stmt.vars) {
        VReg r;
        if (ve.init) {
            if (ve.init->kind == ExprKind::IntLit) {
                auto& e = static_cast<const IntLitExpr&>(*ve.init);
                r = allocVReg();
                func.instrs.push_back(InitVar{r, VRegType::Int64, stoll(e.value)});
            } else {
                r = lowerExpr(*ve.init, func);
            }
        } else {
            r = allocVReg();
        }
        declareVar(ve.varName, r);
    }
}

void PlnVCodeGen::lowerStmt(const Stmt& stmt, VFunc& func)
{
    switch (stmt.kind) {
        case StmtKind::Expr:
            lowerExprStmt(static_cast<const ExprStmt&>(stmt), func);
            return;
        case StmtKind::VarDecl:
            lowerVarDeclStmt(static_cast<const VarDeclStmt&>(stmt), func);
            return;
    }
    BOOST_ASSERT(false);
}

// -------- Entry point --------

VProg PlnVCodeGen::generate(const Module& module)
{
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
