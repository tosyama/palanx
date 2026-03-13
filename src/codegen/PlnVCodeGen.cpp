#include "PlnVCodeGen.h"
#include <boost/assert.hpp>

using namespace std;

VReg PlnVCodeGen::allocVReg()
{
    return nextVReg++;
}

// -------- Lower AST to VInstr --------

void PlnVCodeGen::lowerCCCallExpr(const CCCallExpr& expr, VFunc& func)
{
    int argCount = 0;
    for (auto& arg : expr.args) {
        if (auto* e = dynamic_cast<const StrLitExpr*>(arg.get())) {
            VReg r = allocVReg();
            func.instrs.push_back(LeaLabel{r, VRegType::Ptr64, strLiterals.at(e->value)});
        } else {
            BOOST_ASSERT(false);  // not-impl
        }
        ++argCount;
    }
    func.instrs.push_back(CallC{expr.name, argCount});
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

void PlnVCodeGen::lowerStmt(const Stmt& stmt, VFunc& func)
{
    if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
        lowerExprStmt(*s, func);
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
    for (auto& stmt : module.statements) {
        lowerStmt(*stmt, startFunc);
    }
    startFunc.instrs.push_back(ExitCode{0});
    prog.funcs.push_back(move(startFunc));

    return prog;
}
