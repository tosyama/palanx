#include "PlnX86CodeGen.h"
#include <boost/assert.hpp>
#include <cctype>
#include <array>
#include <iostream>
#include <cstdlib>

using namespace std;

static const char* movInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:
        case VRegType::Uint8:   return "movb";
        case VRegType::Int16:
        case VRegType::Uint16:  return "movw";
        case VRegType::Int32:
        case VRegType::Uint32:  return "movl";
        case VRegType::Float32: return "movss";
        case VRegType::Float64: return "movsd";
        default:                return "movq";
    }
}

static bool isFloat(VRegType t) {
    return t == VRegType::Float32 || t == VRegType::Float64;
}

static const char* addInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:    return "addb";
        case VRegType::Int16:   return "addw";
        case VRegType::Int32:   return "addl";
        case VRegType::Float32: return "addss";
        case VRegType::Float64: return "addsd";
        default:                return "addq";
    }
}

static const char* subInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:    return "subb";
        case VRegType::Int16:   return "subw";
        case VRegType::Int32:   return "subl";
        case VRegType::Float32: return "subss";
        case VRegType::Float64: return "subsd";
        default:                return "subq";
    }
}

// imulb has no 2-operand form; Int8 multiplication is not supported here.
static const char* mulInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int16:   return "imulw";
        case VRegType::Int32:   return "imull";
        case VRegType::Float32: return "mulss";
        case VRegType::Float64: return "mulsd";
        default:                return "imulq";
    }
}

static const char* negInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:  return "negb";
        case VRegType::Int16: return "negw";
        case VRegType::Int32: return "negl";
        default:              return "negq";
    }
}

static const char* cmpInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:  return "cmpb";
        case VRegType::Int16: return "cmpw";
        case VRegType::Int32: return "cmpl";
        default:              return "cmpq";
    }
}

static const char* setCCForOp(const string& op) {
    if (op == "<")  return "setl";
    if (op == "<=") return "setle";
    if (op == ">")  return "setg";
    if (op == ">=") return "setge";
    if (op == "==") return "sete";
    if (op == "!=") return "setne";
    BOOST_ASSERT(false); return "";
}

// Derive the sized register name from the 64-bit base name and VRegType.
// Classic registers (%rax/%rbx/...): drop 'r' prefix for 32-bit, use bare name for 16/8-bit.
// Extended registers (%r8-%r15): append 'd'/'w'/'b' suffix.
// XMM registers: name is identical regardless of float size (instruction determines the op).
static string sizedRegName(const string& base, VRegType type)
{
    // XMM: name unchanged
    if (base.size() >= 4 && base.substr(0, 4) == "%xmm")
        return base;

    // Extended integer registers: %r8, %r9, %r10, ...  (base[2] is a digit)
    if (base.size() >= 3 && base[1] == 'r' && isdigit((unsigned char)base[2])) {
        string stem = base.substr(1);  // "r8", "r10", ...
        switch (type) {
            case VRegType::Ptr64:
            case VRegType::Int64:
            case VRegType::Uint64: return base;
            case VRegType::Int32:
            case VRegType::Uint32: return "%" + stem + "d";
            case VRegType::Int16:
            case VRegType::Uint16: return "%" + stem + "w";
            case VRegType::Int8:
            case VRegType::Uint8:  return "%" + stem + "b";
            default:               return base;
        }
    }

    // Classic registers: lookup table  [64, 32, 16, 8]
    static const map<string, array<string, 4>> classic = {
        {"%rax", {"%rax", "%eax", "%ax",  "%al"}},
        {"%rbx", {"%rbx", "%ebx", "%bx",  "%bl"}},
        {"%rcx", {"%rcx", "%ecx", "%cx",  "%cl"}},
        {"%rdx", {"%rdx", "%edx", "%dx",  "%dl"}},
        {"%rsi", {"%rsi", "%esi", "%si",  "%sil"}},
        {"%rdi", {"%rdi", "%edi", "%di",  "%dil"}},
        {"%rbp", {"%rbp", "%ebp", "%bp",  "%bpl"}},
        {"%rsp", {"%rsp", "%esp", "%sp",  "%spl"}},
    };
    auto it = classic.find(base);
    if (it != classic.end()) {
        switch (type) {
            case VRegType::Ptr64:
            case VRegType::Int64:
            case VRegType::Uint64: return it->second[0];
            case VRegType::Int32:
            case VRegType::Uint32: return it->second[1];
            case VRegType::Int16:
            case VRegType::Uint16: return it->second[2];
            case VRegType::Int8:
            case VRegType::Uint8:  return it->second[3];
            default:               return base;
        }
    }

    return base;  // fallback
}

// Return the AT&T source operand string for a PhysLoc:
//   stack  → "-8(%rbp)"  (memory reference)
//   reg    → "%rsi"      (sized register name)
static string srcOperand(const PhysLoc& loc)
{
    if (loc.isStack())
        return std::to_string(loc.stackOffset) + "(%rbp)";
    return sizedRegName(loc.base, loc.type);
}

// x86-64 System V ABI physical register lists
static const PhysRegs x86PhysRegs = {
    { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" },
    { "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7" },
    { "%rbx", "%r12", "%r13", "%r14", "%r15" }
};

void PlnX86CodeGen::emit(const VProg& prog)
{
    if (!prog.data.empty() || !prog.floatData.empty()) {
        emitSection(".rodata");
        for (auto& d : prog.data)
            emitStringLiteral(d.label, d.value);
        for (auto& f : prog.floatData) {
            emitLabel(f.label);
            if (f.type == VRegType::Float32)
                out << "\t.float " << f.value << "\n";
            else
                out << "\t.double " << f.value << "\n";
        }
    }

    emitSection(".text");
    for (auto& func : prog.funcs) {
        if (func.isEntry || func.isExport)
            emitGlobal(func.name);
        emitLabel(func.name);

        RegAllocResult ra = allocateRegisters(func, x86PhysRegs);
        const RegMap& rm  = ra.regMap;

        if (func.isEntry) {
            // _start: set up frame only when stack space is needed
            if (ra.frameSize > 0) {
                out << "\tpushq %rbp\n";
                out << "\tmovq %rsp, %rbp\n";
                out << "\tsubq $" << ra.frameSize << ", %rsp\n";
            }
        } else {
            // Regular Palan function: always save %rbp for leave/ret
            out << "\tpushq %rbp\n";
            out << "\tmovq %rsp, %rbp\n";
            if (ra.frameSize > 0)
                out << "\tsubq $" << ra.frameSize << ", %rsp\n";
            // Save callee-saved registers into reserved frame slots (top of frame)
            for (int i = 0; i < (int)ra.usedCalleeSaved.size(); i++)
                out << "\tmovq " << ra.usedCalleeSaved[i] << ", " << -(i + 1) * 8 << "(%rbp)\n";
            // Copy parameters remapped from argument registers to their allocated locations
            for (auto& pc : ra.paramCopies) {
                string src = sizedRegName(pc.srcArgReg, pc.type);
                string dst = srcOperand(pc.dst);
                if (src != dst)
                    out << "\t" << movInstrForType(pc.type) << " " << src << ", " << dst << "\n";
            }
        }

        for (auto& instr : func.instrs) {
            if (auto* i = std::get_if<LeaLabel>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                if (!loc.isStack()) {
                    emitLeaLabel(sizedRegName(loc.base, loc.type), i->label);
                } else {
                    // Spilled dst: load address into scratch %rax then store to stack.
                    out << "\tleaq " << i->label << "(%rip), %rax\n";
                    out << "\tmovq %rax, " << srcOperand(loc) << "\n";
                }
            } else if (auto* i = std::get_if<MovImm>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                if (loc.isStack()) {
                    out << "\t" << movInstrForType(i->type) << " $" << i->value << ", " << srcOperand(loc) << "\n";
                } else {
                    emitMovImm(sizedRegName(loc.base, i->type), i->type, i->value);
                }
            } else if (auto* i = std::get_if<InitVar>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                if (loc.isStack()) {
                    out << "\t" << movInstrForType(i->type) << " $" << i->imm << ", " << loc.stackOffset << "(%rbp)\n";
                } else {
                    out << "\t" << movInstrForType(i->type) << " $" << i->imm << ", " << sizedRegName(loc.base, i->type) << "\n";
                }
            } else if (auto* i = std::get_if<InitVarF>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                const char* mov = movInstrForType(i->type);
                if (loc.isStack()) {
                    // Load float constant into %xmm0 scratch, then store to stack slot.
                    out << "\t" << mov << " " << i->label << "(%rip), %xmm0\n";
                    out << "\t" << mov << " %xmm0, " << loc.stackOffset << "(%rbp)\n";
                } else {
                    out << "\t" << mov << " " << i->label << "(%rip), " << loc.base << "\n";
                }
            } else if (auto* a = std::get_if<Add>(&instr)) {
                if (!rm.count(a->dst)) continue;  // dead: result never used
                const PhysLoc& dst_loc = rm.at(a->dst);
                const PhysLoc& lhs_loc = rm.at(a->lhs);
                if (!dst_loc.isStack()) {
                    string dst_reg = sizedRegName(dst_loc.base, a->type);
                    out << "\t" << movInstrForType(a->type) << " " << srcOperand(lhs_loc) << ", " << dst_reg << "\n";
                    out << "\t" << addInstrForType(a->type) << " " << srcOperand(rm.at(a->rhs)) << ", " << dst_reg << "\n";
                } else {
                    // Spilled dst: route through scratch to avoid mem-mem.
                    string scratch = isFloat(a->type) ? "%xmm8" : sizedRegName("%rax", a->type);
                    string dst_mem = srcOperand(dst_loc);
                    out << "\t" << movInstrForType(a->type) << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
                    out << "\t" << addInstrForType(a->type) << " " << srcOperand(rm.at(a->rhs)) << ", " << scratch << "\n";
                    out << "\t" << movInstrForType(a->type) << " " << scratch << ", " << dst_mem << "\n";
                }
            } else if (auto* s = std::get_if<Sub>(&instr)) {
                if (!rm.count(s->dst)) continue;  // dead: result never used
                const PhysLoc& dst_loc = rm.at(s->dst);
                const PhysLoc& lhs_loc = rm.at(s->lhs);
                if (!dst_loc.isStack()) {
                    string dst_reg = sizedRegName(dst_loc.base, s->type);
                    out << "\t" << movInstrForType(s->type) << " " << srcOperand(lhs_loc) << ", " << dst_reg << "\n";
                    out << "\t" << subInstrForType(s->type) << " " << srcOperand(rm.at(s->rhs)) << ", " << dst_reg << "\n";
                } else {
                    // Spilled dst: route through scratch to avoid mem-mem.
                    string scratch = isFloat(s->type) ? "%xmm8" : sizedRegName("%rax", s->type);
                    string dst_mem = srcOperand(dst_loc);
                    out << "\t" << movInstrForType(s->type) << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
                    out << "\t" << subInstrForType(s->type) << " " << srcOperand(rm.at(s->rhs)) << ", " << scratch << "\n";
                    out << "\t" << movInstrForType(s->type) << " " << scratch << ", " << dst_mem << "\n";
                }
            } else if (auto* m = std::get_if<Mul>(&instr)) {
                if (!rm.count(m->dst)) continue;  // dead: result never used
                const PhysLoc& dst_loc = rm.at(m->dst);
                const PhysLoc& lhs_loc = rm.at(m->lhs);
                if (!dst_loc.isStack()) {
                    string dst_reg = sizedRegName(dst_loc.base, m->type);
                    out << "\t" << movInstrForType(m->type) << " " << srcOperand(lhs_loc) << ", " << dst_reg << "\n";
                    out << "\t" << mulInstrForType(m->type) << " " << srcOperand(rm.at(m->rhs)) << ", " << dst_reg << "\n";
                } else {
                    // Spilled dst: route through scratch to avoid mem-mem.
                    string scratch = isFloat(m->type) ? "%xmm8" : sizedRegName("%rax", m->type);
                    string dst_mem = srcOperand(dst_loc);
                    out << "\t" << movInstrForType(m->type) << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
                    out << "\t" << mulInstrForType(m->type) << " " << srcOperand(rm.at(m->rhs)) << ", " << scratch << "\n";
                    out << "\t" << movInstrForType(m->type) << " " << scratch << ", " << dst_mem << "\n";
                }
            } else if (auto* dv = std::get_if<Div>(&instr)) {
                if (!rm.count(dv->dst)) continue;  // dead: result never used
                const PhysLoc& lhs_loc = rm.at(dv->lhs);
                const PhysLoc& rhs_loc = rm.at(dv->rhs);
                const PhysLoc& dst_loc = rm.at(dv->dst);
                if (isFloat(dv->type)) {
                    const char* mov = movInstrForType(dv->type);
                    const char* div = dv->type == VRegType::Float32 ? "divss" : "divsd";
                    out << "\t" << mov << " " << srcOperand(lhs_loc) << ", %xmm8\n";
                    out << "\t" << div << " " << srcOperand(rhs_loc) << ", %xmm8\n";
                    out << "\t" << mov << " %xmm8, " << srcOperand(dst_loc) << "\n";
                } else {
                // idivq clobbers %rax and %rdx; save rhs if it lives in either.
                string rhs_src = srcOperand(rhs_loc);
                if (!rhs_loc.isStack() && (rhs_loc.base == "%rax" || rhs_loc.base == "%rdx")) {
                    out << "\tmovq " << rhs_src << ", %r10\n";
                    rhs_src = "%r10";
                }
                out << "\tmovq " << srcOperand(lhs_loc) << ", %rax\n";
                out << "\tcqto\n";
                out << "\tidivq " << rhs_src << "\n";
                string dst_str = srcOperand(dst_loc);
                if (dst_str != "%rax")
                    out << "\tmovq %rax, " << dst_str << "\n";
                }
            } else if (auto* md = std::get_if<Mod>(&instr)) {
                if (!rm.count(md->dst)) continue;  // dead: result never used
                const PhysLoc& lhs_loc = rm.at(md->lhs);
                const PhysLoc& rhs_loc = rm.at(md->rhs);
                const PhysLoc& dst_loc = rm.at(md->dst);
                // idivq clobbers %rax and %rdx; save rhs if it lives in either.
                string rhs_src = srcOperand(rhs_loc);
                if (!rhs_loc.isStack() && (rhs_loc.base == "%rax" || rhs_loc.base == "%rdx")) {
                    out << "\tmovq " << rhs_src << ", %r10\n";
                    rhs_src = "%r10";
                }
                out << "\tmovq " << srcOperand(lhs_loc) << ", %rax\n";
                out << "\tcqto\n";
                out << "\tidivq " << rhs_src << "\n";
                string dst_str = srcOperand(dst_loc);
                if (dst_str != "%rdx")
                    out << "\tmovq %rdx, " << dst_str << "\n";
            } else if (auto* n = std::get_if<Neg>(&instr)) {
                if (!rm.count(n->dst)) continue;  // dead: result never used
                const PhysLoc& src_loc = rm.at(n->src);
                const PhysLoc& dst_loc = rm.at(n->dst);
                if (!dst_loc.isStack()) {
                    string dst_reg = sizedRegName(dst_loc.base, n->type);
                    out << "\t" << movInstrForType(n->type) << " " << srcOperand(src_loc) << ", " << dst_reg << "\n";
                    out << "\t" << negInstrForType(n->type) << " " << dst_reg << "\n";
                } else {
                    // Spilled dst: route through scratch %rax to avoid mem-mem.
                    string scratch = sizedRegName("%rax", n->type);
                    string dst_mem = srcOperand(dst_loc);
                    out << "\t" << movInstrForType(n->type) << " " << srcOperand(src_loc) << ", " << scratch << "\n";
                    out << "\t" << negInstrForType(n->type) << " " << scratch << "\n";
                    out << "\t" << movInstrForType(n->type) << " " << scratch << ", " << dst_mem << "\n";
                }
            } else if (auto* cm = std::get_if<Cmp>(&instr)) {
                if (!rm.count(cm->dst)) continue;  // dead
                const PhysLoc& dst_loc  = rm.at(cm->dst);
                const PhysLoc& lhs_loc  = rm.at(cm->lhs);
                // cmpX %rhs, lhs  →  flags = lhs - rhs
                // x86 disallows two memory operands: load lhs into scratch if spilled
                string lhs_operand;
                if (lhs_loc.isStack()) {
                    string scratch = sizedRegName("%rax", cm->type);
                    out << "\t" << movInstrForType(cm->type) << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
                    lhs_operand = scratch;
                } else {
                    lhs_operand = srcOperand(lhs_loc);
                }
                out << "\t" << cmpInstrForType(cm->type)
                    << " " << srcOperand(rm.at(cm->rhs))
                    << ", " << lhs_operand << "\n";
                if (!dst_loc.isStack()) {
                    string dst_byte = sizedRegName(dst_loc.base, VRegType::Int8);
                    string dst_32   = sizedRegName(dst_loc.base, VRegType::Int32);
                    out << "\t" << setCCForOp(cm->op) << " " << dst_byte << "\n";
                    out << "\tmovzbl " << dst_byte << ", " << dst_32 << "\n";
                } else {
                    out << "\t" << setCCForOp(cm->op) << " %al\n";
                    out << "\tmovzbl %al, %eax\n";
                    out << "\tmovl %eax, " << srcOperand(dst_loc) << "\n";
                }
            } else if (auto* c = std::get_if<Convert>(&instr)) {
                if (!rm.count(c->dst)) continue;  // dead
                const PhysLoc& dst_loc = rm.at(c->dst);
                bool dst_is_float = (c->to == VRegType::Float32 || c->to == VRegType::Float64);
                if (dst_loc.isStack()) {
                    // Spilled dst: convert into scratch register, then store to stack slot.
                    // Float results use %xmm8 as scratch; integer results use %rax.
                    // NOTE: %xmm8 is safe only because all float VRegs are currently
                    // stack-allocated (no xmm register allocator).  If XMM registers are
                    // ever assigned to live float VRegs, %xmm8 must be chosen more carefully
                    // (e.g. pick a register not live at this instruction).
                    string scratch = dst_is_float ? "%xmm8" : sizedRegName("%rax", c->to);
                    emitConvert(scratch, rm.at(c->src), c->from, c->to);
                    out << "\t" << movInstrForType(c->to) << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
                } else {
                    string dst_reg = dst_is_float ? dst_loc.base : sizedRegName(dst_loc.base, c->to);
                    emitConvert(dst_reg, rm.at(c->src), c->from, c->to);
                }
            } else if (auto* i = std::get_if<CallC>(&instr)) {
                int n_int_regs = (int)x86PhysRegs.intArgs.size();
                int n_flt_regs = (int)x86PhysRegs.floatArgs.size();
                // Count overflow args (int > 6 or float > 8) to pre-allocate stack space.
                int n_stack = 0, int_count = 0, flt_count = 0;
                for (auto vr : i->args) {
                    const PhysLoc& s = rm.at(vr);
                    bool is_flt = (s.type == VRegType::Float32 || s.type == VRegType::Float64);
                    if (is_flt) {
                        if (flt_count >= n_flt_regs) n_stack++;
                        flt_count++;
                    } else {
                        if (int_count >= n_int_regs) n_stack++;
                        int_count++;
                    }
                }
                int stack_space = 0;
                if (n_stack > 0) {
                    stack_space = ((n_stack * 8) + 15) & ~15;
                    out << "\tsubq $" << stack_space << ", %rsp\n";
                }
                int int_idx = 0, flt_idx = 0, stack_idx = 0;
                for (auto vr : i->args) {
                    const PhysLoc& src_loc = rm.at(vr);
                    bool is_flt = (src_loc.type == VRegType::Float32 || src_loc.type == VRegType::Float64);
                    if (is_flt && flt_idx < n_flt_regs) {
                        string xmm = x86PhysRegs.floatArgs[flt_idx++];
                        string s = srcOperand(src_loc);
                        if (s != xmm)
                            out << "\t" << movInstrForType(src_loc.type) << " " << s << ", " << xmm << "\n";
                    } else if (!is_flt && int_idx < n_int_regs) {
                        string dst = sizedRegName(x86PhysRegs.intArgs[int_idx++], src_loc.type);
                        string s = srcOperand(src_loc);
                        if (s != dst)
                            out << "\t" << movInstrForType(src_loc.type) << " " << s << ", " << dst << "\n";
                    } else {
                        // Overflow to stack: both int and float use 8 bytes per slot.
                        int offset = stack_idx++ * 8;
                        if (is_flt) {
                            flt_idx++;
                            const char* mov = movInstrForType(src_loc.type);
                            if (src_loc.isStack()) {
                                // %xmm8 is safe as scratch here for the same reason as in
                                // the Convert block: all float VRegs are currently stack-allocated.
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
                emitCallC(i->name, flt_idx);
                if (stack_space > 0)
                    out << "\taddq $" << stack_space << ", %rsp\n";
                if (i->dst != -1 && rm.count(i->dst)) {
                    const PhysLoc& dst_loc = rm.at(i->dst);
                    bool ret_is_float = (i->retType == VRegType::Float32 ||
                                         i->retType == VRegType::Float64);
                    // Float return value is in %xmm0; integer return value is in %rax.
                    string ret_reg = ret_is_float ? "%xmm0" : sizedRegName("%rax", i->retType);
                    string dst_reg = ret_is_float ? dst_loc.isStack()
                                                        ? srcOperand(dst_loc)
                                                        : dst_loc.base
                                                  : srcOperand(dst_loc);
                    if (dst_reg != ret_reg)
                        out << "\t" << movInstrForType(i->retType) << " " << ret_reg << ", " << dst_reg << "\n";
                }
            } else if (auto* c = std::get_if<CallPln>(&instr)) {
                int n_regs = (int)x86PhysRegs.intArgs.size();
                // Move arguments into intArgs registers.
                for (int j = 0; j < (int)c->args.size() && j < n_regs; j++) {
                    const PhysLoc& src = rm.at(c->args[j]);
                    string s = srcOperand(src);
                    string d = sizedRegName(x86PhysRegs.intArgs[j], src.type);
                    if (s != d)
                        out << "\t" << movInstrForType(src.type) << " " << s << ", " << d << "\n";
                }
                int n_stack = (int)c->args.size() - n_regs;
                int stack_space = 0;
                if (n_stack > 0) {
                    stack_space = ((n_stack * 8) + 15) & ~15;
                    out << "\tsubq $" << stack_space << ", %rsp\n";
                    for (int j = n_regs; j < (int)c->args.size(); j++) {
                        int offset = (j - n_regs) * 8;
                        const PhysLoc& src_loc = rm.at(c->args[j]);
                        if (src_loc.isStack()) {
                            out << "\tmovq " << srcOperand(src_loc) << ", %r10\n";
                            out << "\tmovq %r10, " << offset << "(%rsp)\n";
                        } else {
                            out << "\tmovq " << srcOperand(src_loc)
                                << ", " << offset << "(%rsp)\n";
                        }
                    }
                }
                out << "\tcall " << c->name << "\n";
                if (stack_space > 0)
                    out << "\taddq $" << stack_space << ", %rsp\n";
                // Move return value(s) to destination(s).
                if (c->dsts.size() == 1 && rm.count(c->dsts[0])) {
                    // Single return: copy from %rax
                    const PhysLoc& dst = rm.at(c->dsts[0]);
                    string rax = sizedRegName("%rax", c->retTypes[0]);
                    string d   = srcOperand(dst);
                    if (rax != d)
                        out << "\t" << movInstrForType(c->retTypes[0]) << " " << rax << ", " << d << "\n";
                } else if (c->dsts.size() > 1) {
                    // Multi-return: copy from intArgs[j] in reverse order to avoid overwrite
                    for (int j = (int)c->dsts.size() - 1; j >= 0; j--) {
                        if (!rm.count(c->dsts[j])) continue;
                        const PhysLoc& dst = rm.at(c->dsts[j]);
                        string src_reg = sizedRegName(x86PhysRegs.intArgs[j], c->retTypes[j]);
                        string d = srcOperand(dst);
                        if (src_reg != d)
                            out << "\t" << movInstrForType(c->retTypes[j]) << " " << src_reg << ", " << d << "\n";
                    }
                }
            } else if (auto* r = std::get_if<RetPln>(&instr)) {
                // Move return value(s) to return registers.
                if (r->rets.size() == 1) {
                    // Single return: copy to %rax
                    const PhysLoc& src = rm.at(r->rets[0]);
                    string s   = srcOperand(src);
                    string rax = sizedRegName("%rax", r->types[0]);
                    if (s != rax)
                        out << "\t" << movInstrForType(r->types[0]) << " " << s << ", " << rax << "\n";
                } else if (r->rets.size() > 1) {
                    // Multi-return: copy to intArgs[j] in forward order
                    for (int j = 0; j < (int)r->rets.size(); j++) {
                        const PhysLoc& src = rm.at(r->rets[j]);
                        string s = srcOperand(src);
                        string dst_reg = sizedRegName(x86PhysRegs.intArgs[j], r->types[j]);
                        if (s != dst_reg)
                            out << "\t" << movInstrForType(r->types[j]) << " " << s << ", " << dst_reg << "\n";
                    }
                }
                // Restore callee-saved registers in reverse order before returning
                for (int i = (int)ra.usedCalleeSaved.size() - 1; i >= 0; i--)
                    out << "\tmovq " << -(i + 1) * 8 << "(%rbp), " << ra.usedCalleeSaved[i] << "\n";
                out << "\tleave\n";
                out << "\tret\n";
            } else if (auto* i = std::get_if<ExitCode>(&instr)) {
                emitExit(i->code);
            } else if (std::get_if<BlockEnter>(&instr)) {
                // no-op
            } else if (std::get_if<BlockLeave>(&instr)) {
                // no-op
            } else if (auto* l = std::get_if<Label>(&instr)) {
                out << l->name << ":\n";
            } else if (auto* j = std::get_if<Jmp>(&instr)) {
                out << "\tjmp " << j->label << "\n";
            } else if (auto* cj = std::get_if<CondJmp>(&instr)) {
                const PhysLoc& loc = rm.at(cj->cond);
                string cond_reg;
                if (loc.isStack()) {
                    out << "\tmovl " << srcOperand(loc) << ", %eax\n";
                    cond_reg = "%eax";
                } else {
                    cond_reg = sizedRegName(loc.base, VRegType::Int32);
                }
                out << "\ttestl " << cond_reg << ", " << cond_reg << "\n";
                out << (cj->jumpIfZero ? "\tje " : "\tjne ") << cj->label << "\n";
            } else if (auto* mv = std::get_if<Mov>(&instr)) {
                // Copy src into dst (variable's canonical stack slot or register).
                if (!rm.count(mv->dst) || !rm.count(mv->src)) continue;
                const PhysLoc& dst_loc = rm.at(mv->dst);
                const PhysLoc& src_loc = rm.at(mv->src);
                string s = srcOperand(src_loc);
                string d = srcOperand(dst_loc);
                if (s == d) continue;  // same location: no-op
                if (!dst_loc.isStack()) {
                    string dst_reg = sizedRegName(dst_loc.base, mv->type);
                    out << "\t" << movInstrForType(mv->type) << " " << s << ", " << dst_reg << "\n";
                } else if (!src_loc.isStack()) {
                    out << "\t" << movInstrForType(mv->type) << " " << s << ", " << d << "\n";
                } else {
                    // Both on stack: route through scratch %rax.
                    string scratch = sizedRegName("%rax", mv->type);
                    out << "\t" << movInstrForType(mv->type) << " " << s << ", " << scratch << "\n";
                    out << "\t" << movInstrForType(mv->type) << " " << scratch << ", " << d << "\n";
                }
            }
        }
    }
}

void PlnX86CodeGen::emitSection(const string& name)
{
    out << "\t.section " << name << "\n";
}

void PlnX86CodeGen::emitGlobal(const string& name)
{
    out << "\t.globl " << name << "\n";
}

void PlnX86CodeGen::emitLabel(const string& name)
{
    out << name << ":\n";
}

void PlnX86CodeGen::emitStringLiteral(const string& label, const string& value)
{
    out << label << ":\n";
    out << "\t.string \"";
    for (char c : value) {
        switch (c) {
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            default:   out << c; break;
        }
    }
    out << "\"\n";
}

void PlnX86CodeGen::emitLeaLabel(const string& reg, const string& label)
{
    out << "\tleaq " << label << "(%rip), " << reg << "\n";
}

void PlnX86CodeGen::emitMovImm(const string& reg, VRegType type, long long value)
{
    out << "\t" << movInstrForType(type) << " $" << value << ", " << reg << "\n";
}

void PlnX86CodeGen::emitConvert(const string& dst, const PhysLoc& src, VRegType from, VRegType to)
{
    // Stack is a memory ref; register is sized by the requested type.
    auto srcAt = [&](VRegType t) -> string {
        if (src.isStack()) return std::to_string(src.stackOffset) + "(%rbp)";
        return sizedRegName(src.base, t);
    };

    // Signed integer widening (movsx family)
    if (from == VRegType::Int8  && to == VRegType::Int16) {
        out << "\tmovsbw " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int8  && to == VRegType::Int32) {
        out << "\tmovsbl " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int8  && to == VRegType::Int64) {
        out << "\tmovsbq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int16 && to == VRegType::Int32) {
        out << "\tmovswl " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int16 && to == VRegType::Int64) {
        out << "\tmovswq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int32 && to == VRegType::Int64) {
        out << "\tmovslq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    // Narrowing: reference lower bits of the source register (or same memory ref)
    if (from == VRegType::Int64 && to == VRegType::Int32) {
        out << "\tmovl " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Int16) {
        out << "\tmovw " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Int8) {
        out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int32 && to == VRegType::Int16) {
        out << "\tmovw " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int32 && to == VRegType::Int8) {
        out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int16 && to == VRegType::Int8) {
        out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return;
    }
    // Float to integer conversion (truncation toward zero)
    if (from == VRegType::Float64 && to == VRegType::Int64) {
        out << "\tcvttsd2siq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Float64 && to == VRegType::Int32) {
        out << "\tcvttsd2sil " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Float32 && to == VRegType::Int64) {
        out << "\tcvttss2siq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Float32 && to == VRegType::Int32) {
        out << "\tcvttss2sil " << srcAt(from) << ", " << dst << "\n"; return;
    }
    // Float precision conversion
    if (from == VRegType::Float32 && to == VRegType::Float64) {
        out << "\tcvtss2sd " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Float64 && to == VRegType::Float32) {
        out << "\tcvtsd2ss " << srcAt(from) << ", " << dst << "\n"; return;
    }
    // Integer to float conversion
    if (from == VRegType::Int32 && to == VRegType::Float64) {
        out << "\tcvtsi2sdl " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Float64) {
        out << "\tcvtsi2sdq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int32 && to == VRegType::Float32) {
        out << "\tcvtsi2ssl " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Float32) {
        out << "\tcvtsi2ssq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    // Smaller signed integers: sign-extend to 64-bit via %rax, then convert.
    if ((from == VRegType::Int8 || from == VRegType::Int16) &&
        (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* sx  = (from == VRegType::Int8) ? "movsbq" : "movswq";
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\t" << sx << " " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dst << "\n";
        return;
    }
    // Unsigned integer widening (movzx family — zero-extend)
    if (from == VRegType::Uint8  && to == VRegType::Uint16) {
        out << "\tmovzbw " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint8  && to == VRegType::Uint32) {
        out << "\tmovzbl " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint8  && to == VRegType::Uint64) {
        out << "\tmovzbq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint16 && to == VRegType::Uint32) {
        out << "\tmovzwl " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint16 && to == VRegType::Uint64) {
        out << "\tmovzwq " << srcAt(from) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint32 && to == VRegType::Uint64) {
        // movl to 32-bit register implicitly zero-extends the upper 32 bits of the 64-bit register.
        out << "\tmovl " << srcAt(from) << ", " << sizedRegName(dst, VRegType::Int32) << "\n"; return;
    }
    // Unsigned narrowing (truncation): reference low bits of the source
    if (from == VRegType::Uint64 && to == VRegType::Uint32) {
        out << "\tmovl " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint64 && to == VRegType::Uint16) {
        out << "\tmovw " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint64 && to == VRegType::Uint8) {
        out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint32 && to == VRegType::Uint16) {
        out << "\tmovw " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint32 && to == VRegType::Uint8) {
        out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return;
    }
    if (from == VRegType::Uint16 && to == VRegType::Uint8) {
        out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return;
    }
    // Unsigned integer to float conversion (uint8/16/32 fit in int64, so cvtsi2s*q is correct after zero-extension)
    if (from == VRegType::Uint8 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovzbq " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dst << "\n";
        return;
    }
    if (from == VRegType::Uint16 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovzwq " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dst << "\n";
        return;
    }
    if (from == VRegType::Uint32 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        // movl zero-extends uint32 into %rax; result fits in int64, so cvtsi2s*q is correct.
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovl " << srcAt(from) << ", %eax\n";
        out << "\t" << cvt << " %rax, " << dst << "\n";
        return;
    }
    if (from == VRegType::Uint64 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        // uint64-to-float requires a branch-based sequence that cannot be emitted inline here.
        // Lower this conversion in PlnVCodeGen using Label/CondJmp/Jmp instructions.
        std::cerr << "PlnX86CodeGen: uint64-to-float must be lowered in PlnVCodeGen\n";
        std::abort();
    }
    // Abort unconditionally — do not rely on BOOST_ASSERT which is a no-op in release builds.
    std::cerr << "PlnX86CodeGen: unsupported conversion\n";
    std::abort();
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

void PlnX86CodeGen::emitExit(int code)
{
    out << "\tmovl $" << code << ", %edi\n";
    out << "\tcall exit\n";
}
