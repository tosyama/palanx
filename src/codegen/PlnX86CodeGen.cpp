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

        if (ra.frameSize > 0) {
            out << "\tpushq %rbp\n";
            out << "\tmovq %rsp, %rbp\n";
            out << "\tsubq $" << ra.frameSize << ", %rsp\n";
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
                BOOST_ASSERT(!dst_loc.isStack());  // dst must be in a register
                string dst_reg = sizedRegName(dst_loc.base, a->type);
                out << "\t" << movInstrForType(a->type) << " " << srcOperand(rm.at(a->lhs)) << ", " << dst_reg << "\n";
                out << "\t" << addInstrForType(a->type) << " " << srcOperand(rm.at(a->rhs)) << ", " << dst_reg << "\n";
            } else if (auto* c = std::get_if<Convert>(&instr)) {
                if (!rm.count(c->dst)) continue;  // dead
                const PhysLoc& dst_loc = rm.at(c->dst);
                BOOST_ASSERT(!dst_loc.isStack());
                string dst_reg = sizedRegName(dst_loc.base, c->to);
                emitConvert(dst_reg, srcOperand(rm.at(c->src)), c->from, c->to);
            } else if (auto* i = std::get_if<CallC>(&instr)) {
                for (int j = 0; j < (int)i->args.size(); j++) {
                    const PhysLoc& src_loc = rm.at(i->args[j]);
                    string src = srcOperand(src_loc);
                    string dst = sizedRegName(x86PhysRegs.intArgs[j], src_loc.type);
                    if (src != dst)
                        out << "\t" << movInstrForType(src_loc.type) << " " << src << ", " << dst << "\n";
                }
                emitCallC(i->name);
            } else if (auto* i = std::get_if<ExitCode>(&instr)) {
                emitExit(i->code);
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

void PlnX86CodeGen::emitConvert(const string& dst, const string& src, VRegType from, VRegType to)
{
    // Signed integer widening (movsx family)
    if (from == VRegType::Int8  && to == VRegType::Int16) { out << "\tmovsbw " << src << ", " << dst << "\n"; return; }
    if (from == VRegType::Int8  && to == VRegType::Int32) { out << "\tmovsbl " << src << ", " << dst << "\n"; return; }
    if (from == VRegType::Int8  && to == VRegType::Int64) { out << "\tmovsbq " << src << ", " << dst << "\n"; return; }
    if (from == VRegType::Int16 && to == VRegType::Int32) { out << "\tmovswl " << src << ", " << dst << "\n"; return; }
    if (from == VRegType::Int16 && to == VRegType::Int64) { out << "\tmovswq " << src << ", " << dst << "\n"; return; }
    if (from == VRegType::Int32 && to == VRegType::Int64) { out << "\tmovslq " << src << ", " << dst << "\n"; return; }
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
