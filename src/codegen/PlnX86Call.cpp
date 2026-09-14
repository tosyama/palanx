/// x86-64 call emission: C calls, Palan calls, and Palan returns (ABI argument
/// shuffling and return-value placement).
///
/// @file PlnX86Call.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"

using namespace std;

// One leg of a register-argument shuffle: move `srcOperand` (a register or memory
// operand) into the physical register `dstBase`. `srcBase` is the source's register
// base name when the source is itself a register (empty for a memory operand), used
// to detect when one move's destination is needed as another move's source.
struct RegMove {
    const char* movInstr;
    VRegType    type;
    string      srcOperand;
    string      srcBase;
    string      dstBase;
    string      dstSized;
};

static RegMove makeIntArgMove(const PhysLoc& src_loc, const string& dstBase)
{
    return RegMove{
        movInstrForType(src_loc.type), src_loc.type,
        srcOperand(src_loc), src_loc.isStack() ? "" : src_loc.base,
        dstBase, sizedRegName(dstBase, src_loc.type)
    };
}

// Sequence a set of moves into distinct physical registers so that no move clobbers
// a register another pending move still needs to read from. A naive argument-order
// pass breaks whenever a source register coincides with an earlier argument's
// destination register -- e.g. `g(b, a)` where parameters a/b are homed at %rdi/%rsi
// respectively: moving b into %rdi (position 0) would destroy a's value before it is
// read for position 1. Moves whose destination nothing else needs are emitted
// immediately; any remaining moves form a cycle, broken by stashing one destination's
// live value in `scratch` before it is overwritten.
static void emitSafeRegMoves(ostream& out, vector<RegMove> moves, const string& scratch)
{
    vector<bool> done(moves.size(), false);
    size_t remaining = moves.size();
    while (remaining > 0) {
        bool progress = false;
        for (size_t i = 0; i < moves.size(); i++) {
            if (done[i]) continue;
            bool needed = false;
            for (size_t j = 0; j < moves.size(); j++) {
                if (j == i || done[j]) continue;
                if (!moves[j].srcBase.empty() && moves[j].srcBase == moves[i].dstBase) { needed = true; break; }
            }
            if (!needed) {
                if (moves[i].srcOperand != moves[i].dstSized)
                    out << "\t" << moves[i].movInstr << " " << moves[i].srcOperand << ", " << moves[i].dstSized << "\n";
                done[i] = true;
                remaining--;
                progress = true;
            }
        }
        if (!progress) {
            size_t i = 0;
            while (done[i]) i++;
            out << "\tmovq " << moves[i].dstBase << ", " << scratch << "\n";
            for (size_t j = 0; j < moves.size(); j++) {
                if (done[j] || moves[j].srcBase != moves[i].dstBase) continue;
                moves[j].srcBase    = scratch;
                moves[j].srcOperand = sizedRegName(scratch, moves[j].type);
            }
        }
    }
}

void PlnX86CodeGen::emitInstrCallC(const CallC& i, const RegMap& rm)
{
    int n_int_regs = (int)x86PhysRegs.intArgs.size();
    int n_flt_regs = (int)x86PhysRegs.floatArgs.size();
    // Count overflow args (int > 6 or float > 8) to pre-allocate stack space.
    int n_stack = 0, int_count = 0, flt_count = 0;
    for (auto vr : i.args) {
        const PhysLoc& s = rm.at(vr);
        bool is_flt = (s.type == VRegType::Float32 || s.type == VRegType::Float64);
        if (is_flt) { if (flt_count >= n_flt_regs) n_stack++; flt_count++; }
        else         { if (int_count >= n_int_regs) n_stack++; int_count++; }
    }
    int stack_space = 0;
    if (n_stack > 0) {
        stack_space = ((n_stack * 8) + 15) & ~15;
        out << "\tsubq $" << stack_space << ", %rsp\n";
    }
    // Int register-destined args are queued (see emitSafeRegMoves below) rather
    // than emitted here in argument order, and stack-destined args are still
    // emitted immediately: since no register move has been emitted yet at that
    // point, a stack arg whose source happens to be a register is read safely
    // regardless of which position that register is also a destination for.
    vector<RegMove> intMoves;
    int int_idx = 0, flt_idx = 0, stack_idx = 0;
    for (auto vr : i.args) {
        const PhysLoc& src_loc = rm.at(vr);
        bool is_flt = (src_loc.type == VRegType::Float32 || src_loc.type == VRegType::Float64);
        if (is_flt && flt_idx < n_flt_regs) {
            string xmm = x86PhysRegs.floatArgs[flt_idx++];
            string s = srcOperand(src_loc);
            if (s != xmm)
                out << "\t" << movInstrForType(src_loc.type) << " " << s << ", " << xmm << "\n";
        } else if (!is_flt && int_idx < n_int_regs) {
            intMoves.push_back(makeIntArgMove(src_loc, x86PhysRegs.intArgs[int_idx++]));
        } else {
            // Overflow to stack: both int and float use 8 bytes per slot.
            int offset = stack_idx++ * 8;
            if (is_flt) {
                flt_idx++;
                const char* mov = movInstrForType(src_loc.type);
                if (src_loc.isStack()) {
                    // %xmm8 is safe as scratch here for the same reason as in
                    // emitInstrConvert: all float VRegs are currently stack-allocated.
                    out << "\t" << mov << " " << srcOperand(src_loc) << ", %xmm8\n";
                    out << "\t" << mov << " %xmm8, " << offset << "(%rsp)\n";
                } else {
                    out << "\t" << mov << " " << srcOperand(src_loc) << ", " << offset << "(%rsp)\n";
                }
            } else {
                int_idx++;
                if (src_loc.isStack()) {
                    out << "\tmovq " << srcOperand(src_loc) << ", %r10\n";
                    out << "\tmovq %r10, " << offset << "(%rsp)\n";
                } else {
                    out << "\tmovq " << srcOperand(src_loc) << ", " << offset << "(%rsp)\n";
                }
            }
        }
    }
    emitSafeRegMoves(out, intMoves, "%r11");
    emitCallC(i.name, flt_idx);
    if (stack_space > 0)
        out << "\taddq $" << stack_space << ", %rsp\n";
    if (i.dst != -1 && rm.count(i.dst)) {
        const PhysLoc& dst_loc = rm.at(i.dst);
        bool ret_is_float = (i.retType == VRegType::Float32 || i.retType == VRegType::Float64);
        // Float return value is in %xmm0; integer return value is in %rax.
        string ret_reg = ret_is_float ? "%xmm0" : sizedRegName("%rax", i.retType);
        string dst_reg = ret_is_float ? (dst_loc.isStack() ? srcOperand(dst_loc) : dst_loc.base)
                                      : srcOperand(dst_loc);
        if (dst_reg != ret_reg)
            out << "\t" << movInstrForType(i.retType) << " " << ret_reg << ", " << dst_reg << "\n";
    }
}

void PlnX86CodeGen::emitInstrCallPln(const CallPln& c, const RegMap& rm)
{
    int n_regs = (int)x86PhysRegs.intArgs.size();
    // Queue register-destined args for a hazard-safe shuffle (see emitSafeRegMoves)
    // instead of moving them here in argument order -- a source register can
    // coincide with an earlier argument's destination register (e.g. two
    // parameters passed to a call in swapped order).
    vector<RegMove> intMoves;
    for (int j = 0; j < (int)c.args.size() && j < n_regs; j++)
        intMoves.push_back(makeIntArgMove(rm.at(c.args[j]), x86PhysRegs.intArgs[j]));

    int n_stack = (int)c.args.size() - n_regs;
    int stack_space = 0;
    if (n_stack > 0) {
        stack_space = ((n_stack * 8) + 15) & ~15;
        out << "\tsubq $" << stack_space << ", %rsp\n";
        for (int j = n_regs; j < (int)c.args.size(); j++) {
            int offset = (j - n_regs) * 8;
            const PhysLoc& src_loc = rm.at(c.args[j]);
            if (src_loc.isStack()) {
                out << "\tmovq " << srcOperand(src_loc) << ", %r10\n";
                out << "\tmovq %r10, " << offset << "(%rsp)\n";
            } else {
                out << "\tmovq " << srcOperand(src_loc) << ", " << offset << "(%rsp)\n";
            }
        }
    }
    // Emitted after the stack-arg moves above (which only ever read registers,
    // never write them) so this shuffle is the first thing to touch any register.
    emitSafeRegMoves(out, intMoves, "%r11");
    out << "\tcall " << c.name << "\n";
    if (stack_space > 0)
        out << "\taddq $" << stack_space << ", %rsp\n";
    // Move return value(s) to destination(s).
    if (c.dsts.size() == 1 && rm.count(c.dsts[0])) {
        // Single return: copy from %rax
        const PhysLoc& dst = rm.at(c.dsts[0]);
        string rax = sizedRegName("%rax", c.retTypes[0]);
        string d   = srcOperand(dst);
        if (rax != d)
            out << "\t" << movInstrForType(c.retTypes[0]) << " " << rax << ", " << d << "\n";
    } else if (c.dsts.size() > 1) {
        // Multi-return: copy from intArgs[j] in reverse order to avoid overwrite
        for (int j = (int)c.dsts.size() - 1; j >= 0; j--) {
            if (!rm.count(c.dsts[j])) continue;
            const PhysLoc& dst = rm.at(c.dsts[j]);
            string src_reg = sizedRegName(x86PhysRegs.intArgs[j], c.retTypes[j]);
            string d = srcOperand(dst);
            if (src_reg != d)
                out << "\t" << movInstrForType(c.retTypes[j]) << " " << src_reg << ", " << d << "\n";
        }
    }
}

void PlnX86CodeGen::emitInstrRetPln(const RetPln& r, const RegMap& rm, const vector<string>& usedCalleeSaved)
{
    // Move return value(s) to return registers.
    if (r.rets.size() == 1) {
        // Single return: copy to %rax
        const PhysLoc& src = rm.at(r.rets[0]);
        string s   = srcOperand(src);
        string rax = sizedRegName("%rax", r.types[0]);
        if (s != rax)
            out << "\t" << movInstrForType(r.types[0]) << " " << s << ", " << rax << "\n";
    } else if (r.rets.size() > 1) {
        // Multi-return: copy to intArgs[j] in forward order
        for (int j = 0; j < (int)r.rets.size(); j++) {
            const PhysLoc& src = rm.at(r.rets[j]);
            string s = srcOperand(src);
            string dst_reg = sizedRegName(x86PhysRegs.intArgs[j], r.types[j]);
            if (s != dst_reg)
                out << "\t" << movInstrForType(r.types[j]) << " " << s << ", " << dst_reg << "\n";
        }
    }
    // Restore callee-saved registers in reverse order before returning
    for (int i = (int)usedCalleeSaved.size() - 1; i >= 0; i--)
        out << "\tmovq " << -(i + 1) * 8 << "(%rbp), " << usedCalleeSaved[i] << "\n";
    out << "\tleave\n";
    out << "\tret\n";
}

void PlnX86CodeGen::emitCallC(const string& name, int nFloatArgs)
{
    // For variadic C functions, al = number of XMM registers used for float args.
    if (nFloatArgs == 0)
        out << "\txorl %eax, %eax\n";
    else
        out << "\tmovl $" << nFloatArgs << ", %eax\n";
    out << "\tcall " << name << "\n";
}
