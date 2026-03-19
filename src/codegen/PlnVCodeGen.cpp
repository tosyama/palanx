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
            func.instrs.push_back(MovImm{r, e.type, stoll(e.value)});
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
            func.instrs.push_back(Add{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::CCCall: {
            auto& e = static_cast<const CCCallExpr&>(expr);
            BOOST_ASSERT(e.hasRet);
            vector<VReg> args;
            for (auto& arg : e.args)
                args.push_back(lowerExpr(*arg, func));
            VReg dst = allocVReg();
            func.instrs.push_back(CallC{e.name, move(args), dst, e.retType});
            return dst;
        }
        case ExprKind::PlnCall: {
            auto& e = static_cast<const PlnCallExpr&>(expr);
            BOOST_ASSERT(e.hasRet);
            vector<VReg> args;
            for (auto& a : e.args)
                args.push_back(lowerExpr(*a, func));
            VReg dst = allocVReg();
            func.instrs.push_back(CallPln{e.name, move(args), {dst}, {e.retType}});
            return dst;
        }
        default:
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
    func.instrs.push_back(CallC{expr.name, move(args), /*dst=*/-1});
}

void PlnVCodeGen::lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func)
{
    vector<VReg> args;
    for (auto& a : expr.args)
        args.push_back(lowerExpr(*a, func));
    func.instrs.push_back(CallPln{expr.name, move(args), {}, {}});
}

void PlnVCodeGen::lowerAssignStmt(const AssignStmt& stmt, VFunc& func)
{
    VReg src = lowerExpr(*stmt.value, func);
    varScopes.back()[stmt.name] = src;  // rebind: subsequent uses refer to src
}

void PlnVCodeGen::lowerReturnStmt(const ReturnStmt& stmt, VFunc& func)
{
    // Multi-return: bare return using retVars
    if (currentPlnFunc_ && !currentPlnFunc_->retVars.empty()) {
        BOOST_ASSERT(stmt.values.empty());
        vector<VReg>     rets;
        vector<VRegType> types;
        for (auto& rv : currentPlnFunc_->retVars) {
            rets.push_back(findVar(rv.name));
            types.push_back(rv.type);
        }
        func.instrs.push_back(RetPln{rets, types});
        return;
    }
    if (!currentPlnFunc_ || !currentPlnFunc_->hasRetType) {
        func.instrs.push_back(RetPln{{}, {}});
        return;
    }
    if (!stmt.values.empty()) {
        // Explicit return with value
        BOOST_ASSERT(stmt.values.size() == 1);
        VReg r = lowerExpr(*stmt.values[0], func);
        func.instrs.push_back(RetPln{{r}, {currentPlnFunc_->retType}});
    } else {
        // Bare return: return named variable's current value
        BOOST_ASSERT(!currentPlnFunc_->retVarName.empty());
        VReg r = findVar(currentPlnFunc_->retVarName);
        func.instrs.push_back(RetPln{{r}, {currentPlnFunc_->retType}});
    }
}

void PlnVCodeGen::lowerTappleDeclStmt(const TappleDeclStmt& stmt, VFunc& func)
{
    vector<VReg> args;
    for (auto& a : stmt.args)
        args.push_back(lowerExpr(*a, func));

    vector<VReg>     dsts;
    vector<VRegType> types;
    for (int j = 0; j < (int)stmt.vars.size(); j++) {
        VReg r = allocVReg();
        dsts.push_back(r);
        types.push_back(stmt.retTypes[j]);
        declareVar(stmt.vars[j].name, r);
    }
    func.instrs.push_back(CallPln{stmt.funcName, move(args), move(dsts), move(types)});
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
                func.instrs.push_back(InitVar{r, e.type, stoll(e.value)});
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
        case StmtKind::Assign:
            lowerAssignStmt(static_cast<const AssignStmt&>(stmt), func);
            return;
        case StmtKind::Return:
            lowerReturnStmt(static_cast<const ReturnStmt&>(stmt), func);
            return;
        case StmtKind::TappleDecl:
            lowerTappleDeclStmt(static_cast<const TappleDeclStmt&>(stmt), func);
            return;
    }
    BOOST_ASSERT(false);
}

// -------- Entry point --------

VProg PlnVCodeGen::generate(const Module& module)
{
    VProg prog;

    // Populate .rodata entries
    for (auto& d : module.strLiterals)
        prog.data.push_back(VStringLiteral{d.label, d.value});

    // Lower Palan user-defined functions before _start
    for (auto& pf : module.functions) {
        VFunc vf;
        vf.name = pf.name;
        enterVarScope();
        for (auto& p : pf.params) {
            VReg r = allocVReg();
            vf.params.push_back({r, p.type});
            declareVar(p.name, r);
        }
        // Pre-declare single named return variable
        if (!pf.retVarName.empty()) {
            VReg r = allocVReg();
            vf.instrs.push_back(InitVar{r, pf.retType, 0});
            declareVar(pf.retVarName, r);
        }
        // Pre-declare multiple named return variables
        for (auto& rv : pf.retVars) {
            VReg r = allocVReg();
            vf.instrs.push_back(InitVar{r, rv.type, 0});
            declareVar(rv.name, r);
        }
        currentPlnFunc_ = &pf;
        for (auto& s : pf.body)
            lowerStmt(*s, vf);
        currentPlnFunc_ = nullptr;
        // Append implicit return if the last instruction is not RetPln.
        if (vf.instrs.empty() || !std::get_if<RetPln>(&vf.instrs.back())) {
            if (!pf.retVars.empty()) {
                vector<VReg>     rets;
                vector<VRegType> types;
                for (auto& rv : pf.retVars) {
                    rets.push_back(findVar(rv.name));
                    types.push_back(rv.type);
                }
                vf.instrs.push_back(RetPln{rets, types});
            } else if (pf.hasRetType && !pf.retVarName.empty()) {
                VReg r = findVar(pf.retVarName);
                vf.instrs.push_back(RetPln{{r}, {pf.retType}});
            } else {
                vf.instrs.push_back(RetPln{{}, {}});
            }
        }
        leaveVarScope();
        prog.funcs.push_back(move(vf));
    }

    // Build _start function
    VFunc startFunc;
    startFunc.name = "_start";
    enterVarScope();
    for (auto& stmt : module.statements)
        lowerStmt(*stmt, startFunc);
    leaveVarScope();
    startFunc.isEntry = true;
    startFunc.instrs.push_back(ExitCode{0});
    prog.funcs.push_back(move(startFunc));

    return prog;
}
