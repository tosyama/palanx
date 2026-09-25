/// x86-64 arithmetic, comparison, and type-conversion emission.
///
/// @file PlnX86Arith.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"
#include <iostream>
#include <cstdlib>

using namespace std;

void PlnX86CodeGen::emitBinArith(const string& op, VReg dst, VReg lhs, VReg rhs, VRegType type, const RegMap& rm)
{
    if (!rm.count(dst)) return;  // dead: result never used
    const PhysLoc& dst_loc = rm.at(dst);
    const PhysLoc& lhs_loc = rm.at(lhs);
    const char* mov = movInstrForType(type);
    if (!dst_loc.isStack()) {
        string dst_reg = sizedRegName(dst_loc.base, type);
        out << "\t" << mov << " " << srcOperand(lhs_loc) << ", " << dst_reg << "\n";
        out << "\t" << op  << " " << srcOperand(rm.at(rhs)) << ", " << dst_reg << "\n";
    } else {
        // Spilled dst: route through scratch to avoid mem-mem.
        string scratch = isFloat(type) ? "%xmm8" : sizedRegName("%rax", type);
        out << "\t" << mov << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
        out << "\t" << op  << " " << srcOperand(rm.at(rhs)) << ", " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrMul(const Mul& ml, const RegMap& rm)
{
    if (isFloat(ml.type) || intWidth(ml.type) != 1) {
        emitBinArith(mulInstrForType(ml.type), ml.dst, ml.lhs, ml.rhs, ml.type, rm);
        return;
    }
    if (!rm.count(ml.dst)) return;  // dead: result never used
    // imulb has no 2-operand form. The product's low byte depends only on the
    // operands' low bytes, so a 32-bit multiply of zero-extended operands is exact.
    // rhs is loaded first so a dst sharing rhs's register cannot clobber it.
    const PhysLoc& dst_loc = rm.at(ml.dst);
    out << "\tmovzbl " << srcOperand(rm.at(ml.rhs)) << ", %r10d\n";
    string acc = dst_loc.isStack() ? "%eax" : sizedRegName(dst_loc.base, VRegType::Int32);
    out << "\tmovzbl " << srcOperand(rm.at(ml.lhs)) << ", " << acc << "\n";
    out << "\timull %r10d, " << acc << "\n";
    if (dst_loc.isStack())
        out << "\tmovb %al, " << srcOperand(dst_loc) << "\n";
}

void PlnX86CodeGen::emitIntDivMod(VReg dst, VReg lhs, VReg rhs, VRegType type, const char* result_reg, const RegMap& rm)
{
    if (!rm.count(dst)) return;  // dead: result never used
    const PhysLoc& lhs_loc = rm.at(lhs);
    const PhysLoc& rhs_loc = rm.at(rhs);
    const PhysLoc& dst_loc = rm.at(dst);
    int w = intWidth(type);
    bool sign = isSignedInt(type);
    const string mov = intMnemonic("mov", type);
    // Division clobbers %rax and %rdx, so rhs is moved out of them first.
    string rhs_src = srcOperand(rhs_loc);
    VRegType div_type = type;
    if (w < 4) {
        // 8/16-bit operands are divided at 32 bits: idivb leaves the remainder in %ah,
        // and int8 -128/-1 would raise #DE instead of wrapping.
        div_type = sign ? VRegType::Int32 : VRegType::Uint32;
        out << "\t" << extendMnemonic(sign, w, 4) << " " << rhs_src << ", %r10d\n";
        out << "\t" << extendMnemonic(sign, w, 4) << " " << srcOperand(lhs_loc) << ", %eax\n";
        rhs_src = "%r10d";
    } else {
        if (!rhs_loc.isStack() && (rhs_loc.base == "%rax" || rhs_loc.base == "%rdx")) {
            string r10 = sizedRegName("%r10", type);
            out << "\t" << mov << " " << rhs_src << ", " << r10 << "\n";
            rhs_src = r10;
        }
        out << "\t" << mov << " " << srcOperand(lhs_loc) << ", " << sizedRegName("%rax", type) << "\n";
    }
    if (sign)
        out << (w == 8 ? "\tcqto\n" : "\tcltd\n");
    else
        out << "\txorl %edx, %edx\n";
    out << "\t" << intMnemonic(sign ? "idiv" : "div", div_type) << " " << rhs_src << "\n";
    if (dst_loc.isStack() || dst_loc.base != result_reg)
        out << "\t" << mov << " " << sizedRegName(result_reg, type) << ", " << srcOperand(dst_loc) << "\n";
}

void PlnX86CodeGen::emitInstrDiv(const Div& dv, const RegMap& rm)
{
    if (!isFloat(dv.type)) {
        emitIntDivMod(dv.dst, dv.lhs, dv.rhs, dv.type, "%rax", rm);
        return;
    }
    if (!rm.count(dv.dst)) return;  // dead: result never used
    const char* mov = movInstrForType(dv.type);
    const char* div = dv.type == VRegType::Float32 ? "divss" : "divsd";
    out << "\t" << mov << " " << srcOperand(rm.at(dv.lhs)) << ", %xmm8\n";
    out << "\t" << div << " " << srcOperand(rm.at(dv.rhs)) << ", %xmm8\n";
    out << "\t" << mov << " %xmm8, " << srcOperand(rm.at(dv.dst)) << "\n";
}

void PlnX86CodeGen::emitInstrMod(const Mod& md, const RegMap& rm)
{
    emitIntDivMod(md.dst, md.lhs, md.rhs, md.type, "%rdx", rm);
}

void PlnX86CodeGen::emitUnArith(const string& op, VReg dst, VReg src, VRegType type, const RegMap& rm)
{
    if (!rm.count(dst)) return;  // dead: result never used
    const PhysLoc& src_loc = rm.at(src);
    const PhysLoc& dst_loc = rm.at(dst);
    string mov = movInstrForType(type);
    if (!dst_loc.isStack()) {
        string dst_reg = sizedRegName(dst_loc.base, type);
        out << "\t" << mov << " " << srcOperand(src_loc) << ", " << dst_reg << "\n";
        out << "\t" << op  << " " << dst_reg << "\n";
    } else {
        // Spilled dst: route through scratch to avoid mem-mem.
        string scratch = sizedRegName("%rax", type);
        out << "\t" << mov << " " << srcOperand(src_loc) << ", " << scratch << "\n";
        out << "\t" << op  << " " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrNeg(const Neg& n, const RegMap& rm)
{
    if (!rm.count(n.dst)) return;  // dead: result never used
    if (isFloat(n.type)) {
        // float neg: flip sign bit via xorps/xorpd with mask in .rodata
        const PhysLoc& src_loc = rm.at(n.src);
        const PhysLoc& dst_loc = rm.at(n.dst);
        const char* mov  = movInstrForType(n.type);
        const char* xorI = (n.type == VRegType::Float32) ? "xorps" : "xorpd";
        const char* mask = (n.type == VRegType::Float32) ? ".neg_mask_f32" : ".neg_mask_f64";
        out << "\t" << mov  << " " << srcOperand(src_loc) << ", %xmm8\n";
        out << "\t" << xorI << " " << mask << "(%rip), %xmm8\n";
        out << "\t" << mov  << " %xmm8, " << srcOperand(dst_loc) << "\n";
        return;
    }
    emitUnArith(negInstrForType(n.type), n.dst, n.src, n.type, rm);
}

void PlnX86CodeGen::emitInstrBitNot(const BitNot& n, const RegMap& rm)
{
    emitUnArith(notInstrForType(n.type), n.dst, n.src, n.type, rm);
}

void PlnX86CodeGen::emitInstrCmp(const Cmp& cm, const RegMap& rm)
{
    if (!rm.count(cm.dst)) return;  // dead
    const PhysLoc& dst_loc = rm.at(cm.dst);
    const PhysLoc& lhs_loc = rm.at(cm.lhs);
    // cmpX %rhs, lhs  →  flags = lhs - rhs
    // x86 disallows two memory operands: load lhs into scratch if spilled
    string lhs_operand;
    if (lhs_loc.isStack()) {
        string scratch = isFloat(cm.type) ? "%xmm8" : sizedRegName("%rax", cm.type);
        out << "\t" << movInstrForType(cm.type) << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
        lhs_operand = scratch;
    } else {
        lhs_operand = srcOperand(lhs_loc);
    }
    out << "\t" << cmpInstrForType(cm.type) << " " << srcOperand(rm.at(cm.rhs)) << ", " << lhs_operand << "\n";
    if (!dst_loc.isStack()) {
        string dst_byte = sizedRegName(dst_loc.base, VRegType::Int8);
        string dst_32   = sizedRegName(dst_loc.base, VRegType::Int32);
        out << "\t" << setCCForOp(cm.op, cm.type) << " " << dst_byte << "\n";
        out << "\tmovzbl " << dst_byte << ", " << dst_32 << "\n";
    } else {
        out << "\t" << setCCForOp(cm.op, cm.type) << " %al\n";
        out << "\tmovzbl %al, %eax\n";
        out << "\tmovl %eax, " << srcOperand(dst_loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrConvert(const Convert& c, const RegMap& rm)
{
    if (!rm.count(c.dst)) return;  // dead
    const PhysLoc& dst_loc = rm.at(c.dst);
    bool dst_is_float = (c.to == VRegType::Float32 || c.to == VRegType::Float64);
    if (dst_loc.isStack()) {
        // Spilled dst: convert into scratch register, then store to stack slot.
        // Float results use %xmm8 as scratch; integer results use %rax.
        // NOTE: %xmm8 is safe only because all float VRegs are currently
        // stack-allocated (no xmm register allocator).  If XMM registers are
        // ever assigned to live float VRegs, %xmm8 must be chosen more carefully
        // (e.g. pick a register not live at this instruction).
        string scratchBase = dst_is_float ? "%xmm8" : "%rax";
        emitConvert(scratchBase, rm.at(c.src), c.from, c.to);
        string scratch = dst_is_float ? scratchBase : sizedRegName(scratchBase, c.to);
        out << "\t" << movInstrForType(c.to) << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
    } else {
        emitConvert(dst_loc.base, rm.at(c.src), c.from, c.to);
    }
}

void PlnX86CodeGen::emitConvert(const string& dstBase, const PhysLoc& src, VRegType from, VRegType to)
{
    // Stack is a memory ref; register is sized by the requested type.
    auto srcAt = [&](VRegType t) -> string {
        if (src.isStack()) return std::to_string(src.stackOffset) + "(%rbp)";
        return sizedRegName(src.base, t);
    };
    // Destination is always a register; XMM register names ignore width.
    auto dstAt = [&](VRegType t) -> string {
        if (isFloat(t)) return dstBase;
        return sizedRegName(dstBase, t);
    };

    // Integer <-> integer, any width and signedness.
    if (!isFloat(from) && !isFloat(to)) {
        int fw = intWidth(from), tw = intWidth(to);
        if (tw <= fw) {
            // Narrowing, or same-width sign reinterpretation (e.g. int32<->uint32):
            // the bit pattern is unchanged, so just reference the low bits.
            out << "\t" << movInstrForType(to) << " " << srcAt(to) << ", " << dstAt(to) << "\n";
        } else if (fw == 4 && !isSignedInt(from)) {
            // No movzlq instruction exists: movl into the 32-bit destination
            // register implicitly zero-extends the upper 32 bits of the 64-bit register.
            out << "\tmovl " << srcAt(from) << ", " << dstAt(VRegType::Int32) << "\n";
        } else {
            // Widening: extend according to the SOURCE's signedness (C semantics).
            out << "\t" << extendMnemonic(isSignedInt(from), fw, tw)
                << " " << srcAt(from) << ", " << dstAt(to) << "\n";
        }
        return;
    }

    // Float to integer conversion (truncation toward zero)
    if (isFloat(from) && !isFloat(to)) {
        if (to == VRegType::Uint64) {
            // Requires a branch-based bias-correction sequence that cannot be
            // emitted inline here. Lower this conversion in PlnVCodeGen instead.
            std::cerr << "PlnX86CodeGen: float-to-uint64 conversion must be lowered in PlnVCodeGen\n";
            std::abort();
        }
        // cvtt*2si has only 32-bit and 64-bit integer destination forms.
        // Uint8/16/32 use the 64-bit form: a uint32 above INT32_MAX would not
        // fit the signed 32-bit result of the 32-bit form. Int8/16 reuse the
        // 32-bit form and take their answer from the destination's low bits.
        bool use64 = (to == VRegType::Int64) || !isSignedInt(to);
        const char* cvt = (from == VRegType::Float64)
            ? (use64 ? "cvttsd2siq" : "cvttsd2sil")
            : (use64 ? "cvttss2siq" : "cvttss2sil");
        out << "\t" << cvt << " " << srcAt(from) << ", "
            << dstAt(use64 ? VRegType::Int64 : VRegType::Int32) << "\n";
        return;
    }
    // Float precision conversion
    if (from == VRegType::Float32 && to == VRegType::Float64) {
        out << "\tcvtss2sd " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Float64 && to == VRegType::Float32) {
        out << "\tcvtsd2ss " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    // Integer to float conversion
    if (from == VRegType::Int32 && to == VRegType::Float64) {
        out << "\tcvtsi2sdl " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Float64) {
        out << "\tcvtsi2sdq " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Int32 && to == VRegType::Float32) {
        out << "\tcvtsi2ssl " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Float32) {
        out << "\tcvtsi2ssq " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    // Smaller signed integers: sign-extend to 64-bit via %rax, then convert.
    if ((from == VRegType::Int8 || from == VRegType::Int16) &&
        (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* sx  = (from == VRegType::Int8) ? "movsbq" : "movswq";
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\t" << sx << " " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    // Unsigned integer to float conversion (uint8/16/32 fit in int64, so cvtsi2s*q is correct after zero-extension)
    if (from == VRegType::Uint8 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovzbq " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    if (from == VRegType::Uint16 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovzwq " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    if (from == VRegType::Uint32 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        // movl zero-extends uint32 into %rax; result fits in int64, so cvtsi2s*q is correct.
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovl " << srcAt(from) << ", %eax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    if (from == VRegType::Uint64 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        // uint64-to-float requires a branch-based sequence that cannot be emitted inline here.
        // Lower this conversion in PlnVCodeGen using Label/CondJmp/Jmp instructions.
        std::cerr << "PlnX86CodeGen: uint64-to-float conversion must be lowered in PlnVCodeGen\n";
        std::abort();
    }
    // Abort unconditionally — do not rely on BOOST_ASSERT which is a no-op in release builds.
    // Unreachable: every VRegType pair is handled by the branches above.
    std::cerr << "PlnX86CodeGen: unsupported conversion\n"; // LCOV_EXCL_LINE
    std::abort(); // LCOV_EXCL_LINE
}
