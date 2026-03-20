#include "PlnX86CodeGen.h"
#include <boost/assert.hpp>
#include <cctype>
#include <array>

using namespace std;

static const char* movInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:  return "movb";
        case VRegType::Int16: return "movw";
        case VRegType::Int32: return "movl";
        default:              return "movq";
    }
}

static const char* addInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:  return "addb";
        case VRegType::Int16: return "addw";
        case VRegType::Int32: return "addl";
        default:              return "addq";
    }
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
            case VRegType::Int64:  return base;
            case VRegType::Int32:  return "%" + stem + "d";
            case VRegType::Int16:  return "%" + stem + "w";
            case VRegType::Int8:   return "%" + stem + "b";
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
            case VRegType::Int64:  return it->second[0];
            case VRegType::Int32:  return it->second[1];
            case VRegType::Int16:  return it->second[2];
            case VRegType::Int8:   return it->second[3];
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
    if (!prog.data.empty()) {
        emitSection(".rodata");
        for (auto& d : prog.data) {
            emitStringLiteral(d.label, d.value);
        }
    }

    emitSection(".text");
    for (auto& func : prog.funcs) {
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
        }

        for (auto& instr : func.instrs) {
            if (auto* i = std::get_if<LeaLabel>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                emitLeaLabel(sizedRegName(loc.base, loc.type), i->label);
            } else if (auto* i = std::get_if<MovImm>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                emitMovImm(sizedRegName(loc.base, i->type), i->type, i->value);
            } else if (auto* i = std::get_if<InitVar>(&instr)) {
                const PhysLoc& loc = rm.at(i->dst);
                if (loc.isStack()) {
                    out << "\t" << movInstrForType(i->type) << " $" << i->imm << ", " << loc.stackOffset << "(%rbp)\n";
                } else {
                    out << "\t" << movInstrForType(i->type) << " $" << i->imm << ", " << sizedRegName(loc.base, i->type) << "\n";
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
                    // Spilled dst: use memory destination (x86 allows reg/imm src with mem dst).
                    // lhs must be in a register so mov-to-mem is valid.
                    BOOST_ASSERT(!lhs_loc.isStack());
                    string dst_mem = srcOperand(dst_loc);
                    out << "\t" << movInstrForType(a->type) << " " << srcOperand(lhs_loc) << ", " << dst_mem << "\n";
                    out << "\t" << addInstrForType(a->type) << " " << srcOperand(rm.at(a->rhs)) << ", " << dst_mem << "\n";
                }
            } else if (auto* c = std::get_if<Convert>(&instr)) {
                if (!rm.count(c->dst)) continue;  // dead
                const PhysLoc& dst_loc = rm.at(c->dst);
                if (dst_loc.isStack()) {
                    // Spilled dst: convert into %rax scratch, then store to stack slot.
                    string scratch = sizedRegName("%rax", c->to);
                    emitConvert(scratch, rm.at(c->src), c->from, c->to);
                    out << "\t" << movInstrForType(c->to) << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
                } else {
                    string dst_reg = sizedRegName(dst_loc.base, c->to);
                    emitConvert(dst_reg, rm.at(c->src), c->from, c->to);
                }
            } else if (auto* i = std::get_if<CallC>(&instr)) {
                for (int j = 0; j < (int)i->args.size(); j++) {
                    const PhysLoc& src_loc = rm.at(i->args[j]);
                    string src = srcOperand(src_loc);
                    string dst = sizedRegName(x86PhysRegs.intArgs[j], src_loc.type);
                    if (src != dst)
                        out << "\t" << movInstrForType(src_loc.type) << " " << src << ", " << dst << "\n";
                }
                emitCallC(i->name);
                if (i->dst != -1 && rm.count(i->dst)) {
                    const PhysLoc& dst_loc = rm.at(i->dst);
                    string dst_reg = srcOperand(dst_loc);
                    string rax     = sizedRegName("%rax", i->retType);
                    if (dst_reg != rax)
                        out << "\t" << movInstrForType(i->retType) << " " << rax << ", " << dst_reg << "\n";
                }
            } else if (auto* c = std::get_if<CallPln>(&instr)) {
                // Move arguments into intArgs registers.
                for (int j = 0; j < (int)c->args.size(); j++) {
                    const PhysLoc& src = rm.at(c->args[j]);
                    string s = srcOperand(src);
                    string d = sizedRegName(x86PhysRegs.intArgs[j], src.type);
                    if (s != d)
                        out << "\t" << movInstrForType(src.type) << " " << s << ", " << d << "\n";
                }
                out << "\tcall " << c->name << "\n";
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
    if (from == VRegType::Int8  && to == VRegType::Int16) { out << "\tmovsbw " << srcAt(from) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int8  && to == VRegType::Int32) { out << "\tmovsbl " << srcAt(from) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int8  && to == VRegType::Int64) { out << "\tmovsbq " << srcAt(from) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int16 && to == VRegType::Int32) { out << "\tmovswl " << srcAt(from) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int16 && to == VRegType::Int64) { out << "\tmovswq " << srcAt(from) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int32 && to == VRegType::Int64) { out << "\tmovslq " << srcAt(from) << ", " << dst << "\n"; return; }
    // Narrowing: reference lower bits of the source register (or same memory ref)
    if (from == VRegType::Int64 && to == VRegType::Int32) { out << "\tmovl " << srcAt(to) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int64 && to == VRegType::Int16) { out << "\tmovw " << srcAt(to) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int64 && to == VRegType::Int8)  { out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int32 && to == VRegType::Int16) { out << "\tmovw " << srcAt(to) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int32 && to == VRegType::Int8)  { out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return; }
    if (from == VRegType::Int16 && to == VRegType::Int8)  { out << "\tmovb " << srcAt(to) << ", " << dst << "\n"; return; }
    BOOST_ASSERT(false);  // unsupported conversion
}

void PlnX86CodeGen::emitCallC(const string& name)
{
    // For variadic C functions, al = number of floating-point args (0 here)
    out << "\txorl %eax, %eax\n";
    out << "\tcall " << name << "\n";
}

void PlnX86CodeGen::emitExit(int code)
{
    out << "\tmovl $" << code << ", %edi\n";
    out << "\tcall exit\n";
}
