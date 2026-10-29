/// Virtual code generation: AST (Module) -> VProg.
///
/// @file PlnVCodeGen.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnVCodeGen.h"
#include <boost/assert.hpp>

using namespace std;

// An integer literal's text is parsed per the type SA gave it: a uint64 value
// above INT64_MAX would make stoll throw.
static long long intLitImm(const string& value, VRegType type)
{
    if (type == VRegType::Uint8 || type == VRegType::Uint16
     || type == VRegType::Uint32 || type == VRegType::Uint64)
        return (long long)stoull(value);
    return stoll(value);
}

// k when `e` is an integer literal 2^k with 0 < k < width, else 0.
static int pow2Literal(const Expr& e, VRegType type)
{
    if (isFloat(type) || (e.kind != ExprKind::IntLit && e.kind != ExprKind::UintLit)) return 0;
    const string& text = e.kind == ExprKind::IntLit ? static_cast<const IntLitExpr&>(e).value
                                                    : static_cast<const UintLitExpr&>(e).value;
    if (text[0] == '-') return 0;
    unsigned long long v = stoull(text);
    if (v < 2 || (v & (v - 1))) return 0;
    int k = __builtin_ctzll(v);
    return k < intWidth(type) * 8 ? k : 0;
}

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
        case ExprKind::CGlobal: {
            auto& e = static_cast<const CGlobalExpr&>(expr);
            VReg addr = allocVReg();
            func.instrs.push_back(LeaLabel{addr, VRegType::Ptr64, e.label});
            VReg dst = allocVReg();
            func.instrs.push_back(DerefLoad{dst, addr, 0, e.type});
            return dst;
        }
        case ExprKind::FuncRef: {
            auto& e = static_cast<const FuncRefExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(LeaLabel{r, VRegType::Ptr64, e.name});
            return r;
        }
        case ExprKind::IntLit: {
            auto& e = static_cast<const IntLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, e.type, intLitImm(e.value, e.type)});
            return r;
        }
        case ExprKind::UintLit: {
            auto& e = static_cast<const UintLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, e.type, intLitImm(e.value, e.type)});
            return r;
        }
        case ExprKind::Convert: {
            auto& e = static_cast<const ConvertExpr&>(expr);
            VReg src = lowerExpr(*e.src, func);
            bool fromF = e.from == VRegType::Float32 || e.from == VRegType::Float64;
            bool toF   = e.to   == VRegType::Float32 || e.to   == VRegType::Float64;
            if ((e.from == VRegType::Uint64 && toF) || (fromF && e.to == VRegType::Uint64))
                return lowerU64FloatConvert(src, e.from, e.to, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Convert{dst, src, e.from, e.to});
            return dst;
        }
        case ExprKind::Id: {
            auto& e = static_cast<const IdExpr&>(expr);
            return findVar(e.name);
        }
        case ExprKind::FrameAlloc: {
            auto& e = static_cast<const FrameAllocExpr&>(expr);
            func.frameBlocks.push_back({e.size, e.align});
            VReg dst = allocVReg();
            func.instrs.push_back(FrameAddr{dst, (int)func.frameBlocks.size() - 1});
            return dst;
        }
        case ExprKind::AddrOf: {
            auto& e = static_cast<const AddrOfExpr&>(expr);
            VReg local = findVar(e.name);
            VReg dst = allocVReg();
            func.instrs.push_back(LeaLocal{dst, local});
            return dst;
        }
        case ExprKind::Add: {
            auto& e  = static_cast<const AddExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Add{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Sub: {
            auto& e  = static_cast<const SubExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Sub{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Mul: {
            auto& e  = static_cast<const MulExpr&>(expr);
            if (int k = pow2Literal(*e.right, e.type))
                return shiftImm(lowerExpr(*e.left, func), k, ShiftOp::Shl, e.type, func);
            if (int k = pow2Literal(*e.left, e.type))
                return shiftImm(lowerExpr(*e.right, func), k, ShiftOp::Shl, e.type, func);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Mul{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Div: {
            auto& e  = static_cast<const DivExpr&>(expr);
            if (int k = pow2Literal(*e.right, e.type))
                return lowerPow2DivMod(lowerExpr(*e.left, func), k, false, e.type, func);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Div{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Mod: {
            auto& e  = static_cast<const ModExpr&>(expr);
            if (int k = pow2Literal(*e.right, e.type))
                return lowerPow2DivMod(lowerExpr(*e.left, func), k, true, e.type, func);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Mod{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Neg: {
            auto& e  = static_cast<const NegExpr&>(expr);
            VReg src = lowerExpr(*e.operand, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Neg{dst, src, e.type});
            if (e.type == VRegType::Float32) prog_->needsF32Neg = true;
            if (e.type == VRegType::Float64) prog_->needsF64Neg = true;
            return dst;
        }
        case ExprKind::BitAnd: {
            auto& e  = static_cast<const BitAndExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitAnd{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::BitOr: {
            auto& e  = static_cast<const BitOrExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitOr{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::BitXor: {
            auto& e  = static_cast<const BitXorExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitXor{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Shift: {
            auto& e  = static_cast<const ShiftExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Shift{dst, l, r, e.isRight ? ShiftOp::Shr : ShiftOp::Shl, e.type});
            return dst;
        }
        case ExprKind::BitNot: {
            auto& e  = static_cast<const BitNotExpr&>(expr);
            VReg src = lowerExpr(*e.operand, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitNot{dst, src, e.type});
            return dst;
        }
        case ExprKind::Cmp: {
            auto& e  = static_cast<const CmpExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Cmp{dst, e.op, l, r, e.operandType}); // LCOV_EXCL_EXCEPTION_BR_LINE
            return dst;
        }
        case ExprKind::FloLit: {
            auto& e = static_cast<const FloLitExpr&>(expr);
            string label = addFloatLiteral(e.value, e.type);
            VReg r = allocVReg();
            func.instrs.push_back(InitVarF{r, e.type, label});
            return r;
        }
        case ExprKind::CCCall: {
            auto& e = static_cast<const CCCallExpr&>(expr);
            // A struct-ret call produces no scalar value to plug into an
            // expression context; sa_expression_stmt (E_ByvalStructRetDiscarded)
            // and sa_struct_var_decl are the only SA producers of one, and both
            // always place it as a bare statement (lowerExprStmt), never here.
            BOOST_ASSERT(e.hasRet && !e.hasStructRet);
            vector<StructArgSpan> spans;
            vector<VReg> args = lowerCCallArgs(e, spans, func);
            VReg dst = allocVReg();
            func.instrs.push_back(CallC{e.name, args, {dst}, {e.retType}, std::nullopt, spans}); // LCOV_EXCL_EXCEPTION_BR_LINE
            return dst;
        }
        case ExprKind::PlnCall: {
            auto& e = static_cast<const PlnCallExpr&>(expr);
            BOOST_ASSERT(e.hasRet);
            vector<VReg> args = lowerArgs(e.args, func);
            VReg dst = allocVReg();
            func.instrs.push_back(CallPln{e.name, args, {dst}, {e.retType}}); // LCOV_EXCL_EXCEPTION_BR_LINE
            return dst;
        }
        case ExprKind::PtrCall: {
            auto& e = static_cast<const PtrCallExpr&>(expr);
            BOOST_ASSERT(e.hasRet);
            VReg callee = lowerExpr(*e.callee, func);
            vector<VReg> args = lowerArgs(e.args, func);
            VReg dst = allocVReg();
            func.instrs.push_back(CallC{"", args, {dst}, {e.retType}, callee}); // LCOV_EXCL_EXCEPTION_BR_LINE
            return dst;
        }
        case ExprKind::SysCall: {
            auto& e = static_cast<const SysCallExpr&>(expr);
            BOOST_ASSERT(e.hasRet);
            vector<VReg> args = lowerArgs(e.args, func);
            VReg dst = allocVReg();
            func.instrs.push_back(CallSys{e.sysNum, args, {dst}, {e.retType}}); // LCOV_EXCL_EXCEPTION_BR_LINE
            return dst;
        }
        case ExprKind::LogicalNot: {
            auto& e = static_cast<const LogicalNotExpr&>(expr);
            int idx = labelCounter_++;
            string endLabel = ".Lnot" + to_string(idx) + "_end";

            VReg dst = allocVReg();
            func.instrs.push_back(InitVar{dst, VRegType::Uint8, 1});  // assume true

            VReg src = lowerExpr(*e.operand, func);
            func.instrs.push_back(CondJmp{endLabel, src, true});  // operand==0 → keep 1 // LCOV_EXCL_EXCEPTION_BR_LINE

            VReg zero = allocVReg();
            func.instrs.push_back(MovImm{zero, VRegType::Uint8, 0});
            func.instrs.push_back(Mov{dst, zero, VRegType::Uint8});

            func.instrs.push_back(Label{endLabel});
            return dst;
        }
        case ExprKind::LogicalAnd: {
            auto& e = static_cast<const LogicalAndExpr&>(expr);
            int idx = labelCounter_++;
            string endLabel = ".Lland" + to_string(idx) + "_end";

            VReg dst = allocVReg();
            func.instrs.push_back(InitVar{dst, VRegType::Uint8, 0});

            VReg l = lowerExpr(*e.left, func);
            flushReleases(func);
            func.instrs.push_back(CondJmp{endLabel, l, true});   // l==0 → skip // LCOV_EXCL_EXCEPTION_BR_LINE

            VReg r = lowerExpr(*e.right, func);
            flushReleases(func);
            func.instrs.push_back(CondJmp{endLabel, r, true});   // r==0 → skip // LCOV_EXCL_EXCEPTION_BR_LINE

            VReg one = allocVReg();
            func.instrs.push_back(MovImm{one, VRegType::Uint8, 1});
            func.instrs.push_back(Mov{dst, one, VRegType::Uint8});

            func.instrs.push_back(Label{endLabel});
            return dst;
        }
        case ExprKind::LogicalOr: {
            auto& e = static_cast<const LogicalOrExpr&>(expr);
            int idx = labelCounter_++;
            string trueLabel = ".Llor" + to_string(idx) + "_true";
            string endLabel  = ".Llor" + to_string(idx) + "_end";

            VReg dst = allocVReg();
            func.instrs.push_back(InitVar{dst, VRegType::Uint8, 0});

            VReg l = lowerExpr(*e.left, func);
            flushReleases(func);
            func.instrs.push_back(CondJmp{trueLabel, l, false});  // l!=0 → true // LCOV_EXCL_EXCEPTION_BR_LINE

            VReg r = lowerExpr(*e.right, func);
            flushReleases(func);
            func.instrs.push_back(CondJmp{trueLabel, r, false});  // r!=0 → true // LCOV_EXCL_EXCEPTION_BR_LINE

            func.instrs.push_back(Jmp{endLabel});

            func.instrs.push_back(Label{trueLabel});
            VReg one = allocVReg();
            func.instrs.push_back(MovImm{one, VRegType::Uint8, 1});
            func.instrs.push_back(Mov{dst, one, VRegType::Uint8});

            func.instrs.push_back(Label{endLabel});
            return dst;
        }
        case ExprKind::Temp:
            return lowerTempExpr(static_cast<const TempExpr&>(expr), func);
        case ExprKind::ArrIndex:
            return lowerArrIndexExpr(static_cast<const ArrIndexExpr&>(expr), func);
        case ExprKind::FieldAccess: {
            auto& e   = static_cast<const FieldAccessExpr&>(expr);
            VReg  ptr = e.ptrExpr ? lowerExpr(*e.ptrExpr, func) : findVar(e.varName);
            VReg  dst = allocVReg();
            if (e.addrOnly)
                func.instrs.push_back(CalcAddr{dst, ptr, e.offset});
            else
                func.instrs.push_back(DerefLoad{dst, ptr, e.offset, e.type});
            return dst;
        }
        default:
            BOOST_ASSERT(false);
            return -1;
    }
}

// A width other than 1/2/4/8 is moved as 4-, 2- and 1-byte pieces from the
// low end, so no byte outside the struct is touched.
static vector<pair<int, VRegType>> eightbytePieces(int size)
{
    vector<pair<int, VRegType>> pieces;
    for (int pos = 0; pos < size; ) {
        int w = size - pos >= 4 ? 4 : size - pos >= 2 ? 2 : 1;
        pieces.push_back({pos, w == 4 ? VRegType::Uint32 : w == 2 ? VRegType::Uint16 : VRegType::Uint8});
        pos += w;
    }
    return pieces;
} // LCOV_EXCL_EXCEPTION_BR_LINE

static bool isWholeEightbyte(const Eightbyte& eb)
{
    return isFloat(eb.type) || eb.size == intWidth(eb.type);
}

VReg PlnVCodeGen::loadEightbyte(VReg ptr, int offset, const Eightbyte& eb, VFunc& func)
{
    VReg acc = allocVReg();
    if (isWholeEightbyte(eb)) {
        func.instrs.push_back(DerefLoad{acc, ptr, offset, eb.type});
        return acc;
    }
    bool first = true;
    for (auto [pos, t] : eightbytePieces(eb.size)) {
        VReg piece = allocVReg(), wide = allocVReg();
        func.instrs.push_back(DerefLoad{piece, ptr, offset + pos, t});
        func.instrs.push_back(Convert{wide, piece, t, eb.type});
        if (first) {
            acc = wide;
            first = false;
            continue;
        }
        VReg shifted = allocVReg(), merged = allocVReg();
        func.instrs.push_back(ShiftImm{shifted, wide, 8 * pos, ShiftOp::Shl, eb.type});
        func.instrs.push_back(BitOr{merged, acc, shifted, eb.type});
        acc = merged;
    }
    return acc;
}

void PlnVCodeGen::storeEightbyte(VReg ptr, int offset, VReg src, const Eightbyte& eb, VFunc& func)
{
    if (isWholeEightbyte(eb)) {
        func.instrs.push_back(DerefStore{ptr, offset, src, eb.type});
        return;
    }
    for (auto [pos, t] : eightbytePieces(eb.size)) {
        VReg val = src;
        if (pos > 0) {
            val = allocVReg();
            func.instrs.push_back(ShiftImm{val, src, 8 * pos, ShiftOp::Shr, eb.type});
        }
        func.instrs.push_back(DerefStore{ptr, offset + pos, val, t});
    }
}

// A struct passed by value becomes its eightbytes, read from the struct the
// argument points at; MEMORY class is read the same way, in 8-byte INTEGER
// units, for the emitter to copy to the stack.
vector<VReg> PlnVCodeGen::lowerCCallArgs(const CCCallExpr& expr, vector<StructArgSpan>& spans, VFunc& func)
{
    vector<VReg> args;
    for (size_t i = 0; i < expr.args.size(); i++) {
        VReg v = lowerExpr(*expr.args[i], func);
        if (!expr.structArgs[i]) {
            args.push_back(v);
            continue;
        }
        const CCCallExpr::StructArg& sa = *expr.structArgs[i];
        vector<Eightbyte> ebs = sa.eightbytes;
        bool inMemory = ebs.empty();
        if (inMemory)
            for (int off = 0; off < sa.size; off += 8) {
                int w = std::min(8, sa.size - off);
                ebs.push_back({w == 1 ? VRegType::Int8 : w == 2 ? VRegType::Int16
                               : w <= 4 ? VRegType::Int32 : VRegType::Int64, w});
            }
        spans.push_back({args.size(), ebs.size(), inMemory});
        for (size_t k = 0; k < ebs.size(); k++)
            args.push_back(loadEightbyte(v, 8 * (int)k, ebs[k], func));
    }
    return args;
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnVCodeGen::lowerCCCallExpr(const CCCallExpr& expr, VFunc& func)
{
    vector<StructArgSpan> spans;
    vector<VReg> args = lowerCCallArgs(expr, spans, func);
    if (!expr.hasStructRet || expr.structRetEightbytes.empty()) {
        // Plain void-context call, or a MEMORY-class (>16B) struct return --
        // sa_struct_var_decl already prepended the destination pointer to
        // args as an ordinary first argument for the MEMORY case, so the
        // callee writes the whole struct through it directly; no dsts needed.
        func.instrs.push_back(CallC{expr.name, args, {}, {}, std::nullopt, spans}); // LCOV_EXCL_EXCEPTION_BR_LINE
        return;
    }
    // Register-class struct return (<=16B): the call writes each eightbyte
    // into a fresh vreg (from %rax/%rdx or %xmm0/%xmm1), then a DerefStore
    // per eightbyte copies it into the destination struct's heap storage.
    // var-decl always precedes this call in the same var-decl statement
    // (sa_struct_var_decl), so the destination is already declared.
    VReg dstPtr = findVar(expr.structRetVar);
    vector<VReg> dsts;
    vector<VRegType> types;
    for (const Eightbyte& eb : expr.structRetEightbytes) {
        dsts.push_back(allocVReg());
        types.push_back(eb.type);
    }
    func.instrs.push_back(CallC{expr.name, args, dsts, types, std::nullopt, spans}); // LCOV_EXCL_EXCEPTION_BR_LINE
    for (size_t i = 0; i < dsts.size(); i++)
        storeEightbyte(dstPtr, 8 * (int)i, dsts[i], expr.structRetEightbytes[i], func);
}

void PlnVCodeGen::lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func)
{
    vector<VReg> args = lowerArgs(expr.args, func);
    func.instrs.push_back(CallPln{expr.name, args, {}, {}}); // LCOV_EXCL_EXCEPTION_BR_LINE
}

void PlnVCodeGen::lowerSysCallExpr(const SysCallExpr& expr, VFunc& func)
{
    vector<VReg> args = lowerArgs(expr.args, func);
    func.instrs.push_back(CallSys{expr.sysNum, args, {}, {}}); // LCOV_EXCL_EXCEPTION_BR_LINE
}

vector<VReg> PlnVCodeGen::lowerArgs(const vector<unique_ptr<Expr>>& args, VFunc& func)
{
    vector<VReg> regs;
    for (auto& a : args)
        regs.push_back(lowerExpr(*a, func));
    return regs;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

VReg PlnVCodeGen::lowerTempExpr(const TempExpr& e, VFunc& func)
{
    // Each body statement releases only the temps of its own values; the
    // ones already pending belong to the enclosing full expression.
    vector<const Stmt*> outer = move(pendingReleases_);
    pendingReleases_.clear();
    for (auto& s : e.body)
        lowerStmt(*s, func);
    pendingReleases_ = move(outer);
    for (auto& s : e.release)
        pendingReleases_.push_back(s.get()); // LCOV_EXCL_EXCEPTION_BR_LINE
    return lowerExpr(*e.value, func);
}

void PlnVCodeGen::flushReleases(VFunc& func)
{
    vector<const Stmt*> releases = move(pendingReleases_);
    pendingReleases_.clear();
    for (auto s : releases)
        lowerStmt(*s, func);
}

void PlnVCodeGen::lowerAssignStmt(const AssignStmt& stmt, VFunc& func)
{
    VReg src = lowerExpr(*stmt.value, func);
    // Find the variable's canonical VReg and emit a Mov to update it in place.
    // This keeps the VReg identity stable so that backward references — most
    // importantly, while-loop condition VInstrs that captured the VReg before the
    // loop body ran — always see the current value after each iteration.
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        auto f = it->find(stmt.name);
        if (f != it->end()) {
            VReg old_vreg = f->second;
            if (old_vreg != src)
                func.instrs.push_back(Mov{old_vreg, src, stmt.type});
            return;
        }
    }
    BOOST_ASSERT(false);  // variable not found
}

VReg PlnVCodeGen::lowerArrIndexExpr(const ArrIndexExpr& e, VFunc& func)
{
    VReg base = lowerExpr(*e.array, func);
    VReg idx  = lowerExpr(*e.index, func);
    VReg dst  = allocVReg();

    if (e.scale_expr) {
        // Variable inner dim: runtime stride = scale_expr (e.g. __mat_d1 * sizeof(T))
        VReg stride = lowerExpr(*e.scale_expr, func);
        VReg offset = allocVReg();
        // Widen idx to Int64 before the 64-bit multiply; a narrower spilled vreg
        // would be read as 8 bytes by emitBinArith, corrupting the result.
        VReg idx64 = idx;
        if (e.idx_type != VRegType::Int64 && e.idx_type != VRegType::Uint64) {
            idx64 = allocVReg();
            func.instrs.push_back(Convert{idx64, idx, e.idx_type, VRegType::Int64});
        }
        func.instrs.push_back(Mul{offset, idx64, stride, VRegType::Uint64});
        if (e.addrOnly)
            func.instrs.push_back(Add{dst, base, offset, VRegType::Ptr64});
        else
            func.instrs.push_back(DerefLoadIdx{dst, base, offset, 1, e.type, VRegType::Uint64});
    } else if (e.addrOnly) {
        // Constant-stride row access on embedded 2D array: compute address without dereference.
        func.instrs.push_back(CalcAddrIdx{dst, base, idx, e.scale, e.idx_type});
    } else {
        // Ordinary element access (1D or 2D element via row pointer).
        func.instrs.push_back(DerefLoadIdx{dst, base, idx, e.scale, e.type, e.idx_type});
    }
    return dst;
}

void PlnVCodeGen::lowerArrAssignStmt(const ArrAssignStmt& s, VFunc& func)
{
    VReg base = lowerExpr(*s.array, func);
    VReg idx  = lowerExpr(*s.index, func);
    VReg src  = lowerExpr(*s.value, func);
    func.instrs.push_back(DerefStoreIdx{base, idx, s.scale, src, s.type, s.idx_type});
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
        func.instrs.push_back(RetPln{rets, types}); // LCOV_EXCL_EXCEPTION_BR_LINE
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
        flushReleases(func);
        func.instrs.push_back(RetPln{{r}, {currentPlnFunc_->retType}}); // LCOV_EXCL_EXCEPTION_BR_LINE
    } else {
        // Bare return: return named variable's current value
        BOOST_ASSERT(!currentPlnFunc_->retVarName.empty());
        VReg r = findVar(currentPlnFunc_->retVarName);
        func.instrs.push_back(RetPln{{r}, {currentPlnFunc_->retType}}); // LCOV_EXCL_EXCEPTION_BR_LINE
    }
}

void PlnVCodeGen::lowerTappleDeclStmt(const TappleDeclStmt& stmt, VFunc& func)
{
    vector<VReg> args = lowerArgs(stmt.args, func);

    vector<VReg>     dsts;
    vector<VRegType> types;
    for (int j = 0; j < (int)stmt.vars.size(); j++) {
        VReg r = allocVReg();
        dsts.push_back(r);
        types.push_back(stmt.retTypes[j]);
        declareVar(stmt.vars[j].name, r);
    }
    func.instrs.push_back(CallPln{stmt.funcName, args, move(dsts), move(types)}); // LCOV_EXCL_EXCEPTION_BR_LINE
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
        case ExprKind::SysCall:
            lowerSysCallExpr(static_cast<const SysCallExpr&>(*stmt.body), func);
            return;
        case ExprKind::PtrCall: {
            auto& e = static_cast<const PtrCallExpr&>(*stmt.body);
            VReg callee = lowerExpr(*e.callee, func);
            vector<VReg> args = lowerArgs(e.args, func);
            func.instrs.push_back(CallC{"", args, {}, {}, callee}); // LCOV_EXCL_EXCEPTION_BR_LINE
            return;
        }
        default:
            BOOST_ASSERT(false);  // only call expressions are valid as statements
    }
}

VReg PlnVCodeGen::shiftImm(VReg src, int count, ShiftOp op, VRegType type, VFunc& func)
{
    VReg dst = allocVReg();
    func.instrs.push_back(ShiftImm{dst, src, count, op, type});
    return dst;
}

// Division by 2^k truncates toward zero, while an arithmetic shift rounds
// toward negative infinity: a negative dividend first gets 2^k - 1 added.
// The remainder then follows from the quotient, keeping the dividend's sign.
VReg PlnVCodeGen::lowerPow2DivMod(VReg x, int k, bool isMod, VRegType type, VFunc& func)
{
    if (!isSignedInt(type)) {
        if (!isMod) return shiftImm(x, k, ShiftOp::Shr, type, func);
        VReg mask = allocVReg();
        func.instrs.push_back(MovImm{mask, type, (long long)((1ULL << k) - 1)});
        VReg dst = allocVReg();
        func.instrs.push_back(BitAnd{dst, x, mask, type});
        return dst;
    }
    int bits = intWidth(type) * 8;
    VReg sign = shiftImm(x, bits - 1, ShiftOp::Sar, type, func);
    VReg bias = shiftImm(sign, bits - k, ShiftOp::Shr, type, func);
    VReg adj = allocVReg();
    func.instrs.push_back(Add{adj, x, bias, type});
    VReg q = shiftImm(adj, k, ShiftOp::Sar, type, func);
    if (!isMod) return q;
    VReg back = shiftImm(q, k, ShiftOp::Shl, type, func);
    VReg dst = allocVReg();
    func.instrs.push_back(Sub{dst, x, back, type});
    return dst;
}

// SSE converts only cover the signed int64 range, so values with the top bit
// set need a branch; emitConvert emits straight-line code only.
VReg PlnVCodeGen::lowerU64FloatConvert(VReg src, VRegType from, VRegType to, VFunc& func)
{
    auto imm = [&](VRegType t, long long v) {
        VReg r = allocVReg();
        func.instrs.push_back(MovImm{r, t, v});
        return r;
    };
    auto op = [&](auto instr) {
        func.instrs.push_back(instr);
        return instr.dst;
    };

    bool toFloat = from == VRegType::Uint64;
    string prefix   = (toFloat ? ".Lu2f" : ".Lf2u") + to_string(labelCounter_++);
    string hiLabel  = prefix + "_hi";
    string endLabel = prefix + "_end";
    VReg dst = allocVReg();

    if (toFloat) {
        func.instrs.push_back(InitVarF{dst, to, addFloatLiteral("0.0", to)});

        VReg neg = op(Cmp{allocVReg(), "<", src, imm(VRegType::Int64, 0), VRegType::Int64}); // LCOV_EXCL_EXCEPTION_BR_LINE
        func.instrs.push_back(CondJmp{hiLabel, neg, false}); // LCOV_EXCL_EXCEPTION_BR_LINE
        VReg lo = op(Convert{allocVReg(), src, VRegType::Int64, to});
        func.instrs.push_back(Mov{dst, lo, to});
        func.instrs.push_back(Jmp{endLabel});

        // Halve, keeping the dropped low bit as a sticky bit so the
        // int64->float rounding of the halved value matches that of x.
        func.instrs.push_back(Label{hiLabel});
        VReg half = op(Div{allocVReg(), src, imm(VRegType::Uint64, 2), VRegType::Uint64});
        VReg bit  = op(BitAnd{allocVReg(), src, imm(VRegType::Int64, 1), VRegType::Int64});
        VReg h    = op(BitOr{allocVReg(), half, bit, VRegType::Int64});
        VReg hf   = op(Convert{allocVReg(), h, VRegType::Int64, to});
        VReg hi   = op(Add{allocVReg(), hf, hf, to});
        func.instrs.push_back(Mov{dst, hi, to});
    } else {
        func.instrs.push_back(InitVar{dst, VRegType::Uint64, 0});

        VReg lim = allocVReg();
        func.instrs.push_back(InitVarF{lim, from, addFloatLiteral("9223372036854775808.0", from)});
        VReg ge = op(Cmp{allocVReg(), ">=", src, lim, from}); // LCOV_EXCL_EXCEPTION_BR_LINE
        func.instrs.push_back(CondJmp{hiLabel, ge, false}); // LCOV_EXCL_EXCEPTION_BR_LINE
        VReg lo = op(Convert{allocVReg(), src, from, VRegType::Int64});
        func.instrs.push_back(Mov{dst, lo, VRegType::Uint64});
        func.instrs.push_back(Jmp{endLabel});

        func.instrs.push_back(Label{hiLabel});
        VReg biased = op(Sub{allocVReg(), src, lim, from});
        VReg bi     = op(Convert{allocVReg(), biased, from, VRegType::Int64});
        VReg hi     = op(BitXor{allocVReg(), bi, imm(VRegType::Int64, INT64_MIN), VRegType::Int64});
        func.instrs.push_back(Mov{dst, hi, VRegType::Uint64});
    }
    func.instrs.push_back(Label{endLabel});
    return dst;
}

// Return a float literal label, adding the literal to prog_.floatData.
string PlnVCodeGen::addFloatLiteral(const string& value, VRegType type)
{
    string label = ".LF" + to_string(floatLabelCounter_++);
    prog_->floatData.push_back(VFloatLiteral{label, value, type}); // LCOV_EXCL_EXCEPTION_BR_LINE
    return label;
}

void PlnVCodeGen::lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func)
{
    for (auto& ve : stmt.vars) {
        VReg r;
        if (ve.init) {
            if (ve.init->kind == ExprKind::FloLit) {
                auto& e = static_cast<const FloLitExpr&>(*ve.init);
                string label = addFloatLiteral(e.value, e.type);
                r = allocVReg();
                func.instrs.push_back(InitVarF{r, e.type, label});
                if (!blockVarStack_.empty())
                    blockVarStack_.back().push_back(r);
            } else if (ve.init->kind == ExprKind::IntLit) {
                auto& e = static_cast<const IntLitExpr&>(*ve.init);
                r = allocVReg();
                func.instrs.push_back(InitVar{r, e.type, intLitImm(e.value, e.type)});
                if (!blockVarStack_.empty())
                    blockVarStack_.back().push_back(r);
            } else if (ve.init->kind == ExprKind::UintLit) {
                auto& e = static_cast<const UintLitExpr&>(*ve.init);
                r = allocVReg();
                func.instrs.push_back(InitVar{r, e.type, intLitImm(e.value, e.type)});
                if (!blockVarStack_.empty())
                    blockVarStack_.back().push_back(r);
            } else if (ve.init->kind == ExprKind::Id) {
                // The source's VReg is its canonical storage, updated in place by
                // later assignments; sharing it would alias the two variables.
                r = allocVReg();
                func.instrs.push_back(Copy{r, lowerExpr(*ve.init, func), ve.type});
            } else {
                r = lowerExpr(*ve.init, func);
            }
        } else {
            r = allocVReg();
            func.instrs.push_back(DeclVar{r, ve.type});
            if (!blockVarStack_.empty())
                blockVarStack_.back().push_back(r);
        }
        declareVar(ve.varName, r);
    }
}

void PlnVCodeGen::lowerBlockStmt(const BlockStmt& stmt, VFunc& func)
{
    func.instrs.push_back(BlockEnter{});
    enterVarScope();
    blockVarStack_.push_back({});

    for (auto& s : stmt.body)
        lowerStmt(*s, func);

    vector<VReg> expired = move(blockVarStack_.back());
    blockVarStack_.pop_back();
    leaveVarScope();
    func.instrs.push_back(BlockLeave{move(expired)});
}

void PlnVCodeGen::lowerBranchCond(const Expr& expr, VFunc& func, const string& falseLabel)
{
    if (expr.kind == ExprKind::LogicalNot) {
        auto& e = static_cast<const LogicalNotExpr&>(expr);
        VReg cond = lowerExpr(*e.operand, func);
        flushReleases(func);
        func.instrs.push_back(CondJmp{falseLabel, cond, false});  // jne // LCOV_EXCL_EXCEPTION_BR_LINE
        return;
    }
    if (expr.kind == ExprKind::LogicalAnd) {
        auto& e = static_cast<const LogicalAndExpr&>(expr);
        lowerBranchCond(*e.left,  func, falseLabel);
        lowerBranchCond(*e.right, func, falseLabel);
        return;
    }
    if (expr.kind == ExprKind::LogicalOr) {
        auto& e = static_cast<const LogicalOrExpr&>(expr);
        int idx = labelCounter_++;
        string skipLabel = ".Llor" + to_string(idx) + "_skip";
        lowerBranchCondTrue(*e.left, func, skipLabel);   // left true → skip right check
        lowerBranchCond(*e.right, func, falseLabel);     // right false → jump to falseLabel
        func.instrs.push_back(Label{skipLabel});
        return;
    }
    VReg cond = lowerExpr(expr, func);
    flushReleases(func);
    func.instrs.push_back(CondJmp{falseLabel, cond, true});  // je // LCOV_EXCL_EXCEPTION_BR_LINE
}

void PlnVCodeGen::lowerBranchCondTrue(const Expr& expr, VFunc& func, const string& trueLabel)
{
    if (expr.kind == ExprKind::LogicalNot) {
        auto& e = static_cast<const LogicalNotExpr&>(expr);
        VReg cond = lowerExpr(*e.operand, func);
        flushReleases(func);
        func.instrs.push_back(CondJmp{trueLabel, cond, true});   // je: !x true when x==0 // LCOV_EXCL_EXCEPTION_BR_LINE
        return;
    }
    if (expr.kind == ExprKind::LogicalOr) {
        auto& e = static_cast<const LogicalOrExpr&>(expr);
        lowerBranchCondTrue(*e.left,  func, trueLabel);
        lowerBranchCondTrue(*e.right, func, trueLabel);
        return;
    }
    if (expr.kind == ExprKind::LogicalAnd) {
        auto& e = static_cast<const LogicalAndExpr&>(expr);
        int idx = labelCounter_++;
        string skipLabel = ".Land" + to_string(idx) + "_skip";
        lowerBranchCond(*e.left, func, skipLabel);       // left false → skip right check
        lowerBranchCondTrue(*e.right, func, trueLabel);  // right true → jump to trueLabel
        func.instrs.push_back(Label{skipLabel});
        return;
    }
    VReg cond = lowerExpr(expr, func);
    flushReleases(func);
    func.instrs.push_back(CondJmp{trueLabel, cond, false});  // jne: jump when non-zero // LCOV_EXCL_EXCEPTION_BR_LINE
}

void PlnVCodeGen::lowerIfStmt(const IfStmt& stmt, VFunc& func)
{
    int    idx      = labelCounter_++;
    string endLabel = ".Lif" + to_string(idx) + "_end";

    if (stmt.elseStmt) {
        string elseLabel = ".Lif" + to_string(idx) + "_else";
        lowerBranchCond(*stmt.cond, func, elseLabel);
        lowerStmt(*stmt.thenStmt, func);
        func.instrs.push_back(Jmp{endLabel});
        func.instrs.push_back(Label{elseLabel});
        lowerStmt(*stmt.elseStmt, func);
    } else {
        lowerBranchCond(*stmt.cond, func, endLabel);
        lowerStmt(*stmt.thenStmt, func);
    }

    func.instrs.push_back(Label{endLabel});
}

void PlnVCodeGen::lowerWhileStmt(const WhileStmt& stmt, VFunc& func)
{
    int    idx        = labelCounter_++;
    string startLabel = ".Lwhile" + to_string(idx) + "_start";
    string endLabel   = ".Lwhile" + to_string(idx) + "_end";

    loopStack_.push_back({startLabel, endLabel});
    func.instrs.push_back(Label{startLabel});
    lowerBranchCond(*stmt.cond, func, endLabel);
    lowerStmt(*stmt.body, func);
    func.instrs.push_back(Jmp{startLabel});
    func.instrs.push_back(Label{endLabel});
    loopStack_.pop_back();
}

void PlnVCodeGen::lowerBreakStmt(VFunc& func)
{
    BOOST_ASSERT(!loopStack_.empty());
    func.instrs.push_back(Jmp{loopStack_.back().end});
}

void PlnVCodeGen::lowerContinueStmt(VFunc& func)
{
    BOOST_ASSERT(!loopStack_.empty());
    func.instrs.push_back(Jmp{loopStack_.back().start});
}

void PlnVCodeGen::lowerStmt(const Stmt& stmt, VFunc& func)
{
    switch (stmt.kind) {
        case StmtKind::Expr:
            lowerExprStmt(static_cast<const ExprStmt&>(stmt), func);
            break;
        case StmtKind::VarDecl:
            lowerVarDeclStmt(static_cast<const VarDeclStmt&>(stmt), func);
            break;
        case StmtKind::Assign:
            lowerAssignStmt(static_cast<const AssignStmt&>(stmt), func);
            break;
        case StmtKind::Return:
            lowerReturnStmt(static_cast<const ReturnStmt&>(stmt), func);
            break;
        case StmtKind::TappleDecl:
            lowerTappleDeclStmt(static_cast<const TappleDeclStmt&>(stmt), func);
            break;
        case StmtKind::Block:
            lowerBlockStmt(static_cast<const BlockStmt&>(stmt), func);
            break;
        case StmtKind::If:
            lowerIfStmt(static_cast<const IfStmt&>(stmt), func);
            break;
        case StmtKind::While:
            lowerWhileStmt(static_cast<const WhileStmt&>(stmt), func);
            break;
        case StmtKind::Break:
            lowerBreakStmt(func);
            break;
        case StmtKind::Continue:
            lowerContinueStmt(func);
            break;
        case StmtKind::ArrAssign:
            lowerArrAssignStmt(static_cast<const ArrAssignStmt&>(stmt), func);
            break;
        case StmtKind::FieldAssign: {
            auto& s   = static_cast<const FieldAssignStmt&>(stmt);
            VReg  src = lowerExpr(*s.value, func);
            VReg  ptr = s.ptrExpr ? lowerExpr(*s.ptrExpr, func) : findVar(s.varName);
            func.instrs.push_back(DerefStore{ptr, s.offset, src, s.type});
            break;
        }
        default:
            BOOST_ASSERT(false);
    }
    flushReleases(func);
}

// -------- Entry point --------

VProg PlnVCodeGen::generate(const Module& module)
{
    VProg prog;
    prog_ = &prog;

    // Populate .rodata entries
    for (auto& d : module.strLiterals)
        prog.data.push_back(VStringLiteral{d.label, d.value}); // LCOV_EXCL_EXCEPTION_BR_LINE

    for (auto& pf : module.functions) {
        VFunc vf;
        vf.name = pf.name;
        vf.isExport = pf.isExport;
        enterVarScope();
        for (auto& p : pf.params) {
            VReg r = allocVReg();
            vf.params.push_back({r, p.type});
            declareVar(p.name, r);
        }
        // Pre-declare single named return variable
        auto zeroInit = [&](VReg r, VRegType t) {
            if (t == VRegType::Float32 || t == VRegType::Float64)
                vf.instrs.push_back(InitVarF{r, t, addFloatLiteral("0.0", t)});
            else
                vf.instrs.push_back(InitVar{r, t, 0});
        };
        if (!pf.retVarName.empty()) {
            VReg r = allocVReg();
            zeroInit(r, pf.retType);
            declareVar(pf.retVarName, r);
        }
        // Pre-declare multiple named return variables
        for (auto& rv : pf.retVars) {
            VReg r = allocVReg();
            zeroInit(r, rv.type);
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
                vf.instrs.push_back(RetPln{rets, types}); // LCOV_EXCL_EXCEPTION_BR_LINE
            } else if (pf.hasRetType && !pf.retVarName.empty()) {
                VReg r = findVar(pf.retVarName);
                vf.instrs.push_back(RetPln{{r}, {pf.retType}}); // LCOV_EXCL_EXCEPTION_BR_LINE
            } else {
                vf.instrs.push_back(RetPln{{}, {}});
            }
        }
        leaveVarScope();
        prog.funcs.push_back(move(vf));
    }

    return prog;
}
