#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"

using namespace std;

// x86-64 System V ABI physical register lists
const PhysRegs PlnX86CodeGen::x86PhysRegs = {
    { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" },
    { "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7" },
    { "%rbx", "%r12", "%r13", "%r14", "%r15" },
    { "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9" },
    { "%r10", "%r11" }
};

void PlnX86CodeGen::emit(const VProg& prog, const vector<RegAllocResult>& allocs)
{
    if (!prog.data.empty() || !prog.floatData.empty() || prog.needsF32Neg || prog.needsF64Neg) {
        emitSection(".rodata");
        if (prog.needsF32Neg)
            out << ".neg_mask_f32:\n\t.long 0x80000000\n";
        if (prog.needsF64Neg)
            out << ".neg_mask_f64:\n\t.quad 0x8000000000000000\n";
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
    for (size_t idx = 0; idx < prog.funcs.size(); ++idx) {
        const VFunc& func = prog.funcs[idx];
        const RegAllocResult& ra = allocs[idx];
        const RegMap& rm  = ra.regMap;

        if (func.isEntry || func.isExport)
            emitGlobal(func.name);
        emitLabel(func.name);

        emitFuncPrologue(func, ra, rm);

        for (auto& instr : func.instrs) {
            if      (auto* i  = std::get_if<LeaLabel> (&instr)) emitInstrLeaLabel(*i, rm);
            else if (auto* i  = std::get_if<MovImm>   (&instr)) emitInstrMovImm(*i, rm);
            else if (auto* i  = std::get_if<InitVar>  (&instr)) emitInstrInitVar(*i, rm);
            else if (auto* i  = std::get_if<InitVarF> (&instr)) emitInstrInitVarF(*i, rm);
            else if (auto* a  = std::get_if<Add>      (&instr)) emitBinArith(addInstrForType(a->type), a->dst, a->lhs, a->rhs, a->type, rm);
            else if (auto* s  = std::get_if<Sub>      (&instr)) emitBinArith(subInstrForType(s->type), s->dst, s->lhs, s->rhs, s->type, rm);
            else if (auto* m  = std::get_if<Mul>      (&instr)) emitBinArith(mulInstrForType(m->type), m->dst, m->lhs, m->rhs, m->type, rm);
            else if (auto* i  = std::get_if<Div>      (&instr)) emitInstrDiv(*i, rm);
            else if (auto* i  = std::get_if<Mod>      (&instr)) emitInstrMod(*i, rm);
            else if (auto* i  = std::get_if<Neg>      (&instr)) emitInstrNeg(*i, rm);
            else if (auto* a  = std::get_if<BitAnd>   (&instr)) emitBinArith(andInstrForType(a->type), a->dst, a->lhs, a->rhs, a->type, rm);
            else if (auto* o  = std::get_if<BitOr>    (&instr)) emitBinArith(orInstrForType(o->type),  o->dst, o->lhs, o->rhs, o->type, rm);
            else if (auto* x  = std::get_if<BitXor>   (&instr)) emitBinArith(xorInstrForType(x->type), x->dst, x->lhs, x->rhs, x->type, rm);
            else if (auto* i  = std::get_if<BitNot>   (&instr)) emitInstrBitNot(*i, rm);
            else if (auto* i  = std::get_if<Cmp>      (&instr)) emitInstrCmp(*i, rm);
            else if (auto* i  = std::get_if<Convert>  (&instr)) emitInstrConvert(*i, rm);
            else if (auto* i  = std::get_if<CallC>    (&instr)) emitInstrCallC(*i, rm);
            else if (auto* i  = std::get_if<CallPln>  (&instr)) emitInstrCallPln(*i, rm);
            else if (auto* i  = std::get_if<RetPln>   (&instr)) emitInstrRetPln(*i, rm, ra.usedCalleeSaved);
            else if (auto* i  = std::get_if<ExitCode> (&instr)) emitExit(i->code);
            else if (auto* l  = std::get_if<Label>    (&instr)) out << l->name << ":\n";
            else if (auto* j  = std::get_if<Jmp>      (&instr)) out << "\tjmp " << j->label << "\n";
            else if (auto* i  = std::get_if<CondJmp>  (&instr)) emitInstrCondJmp(*i, rm);
            else if (auto* i  = std::get_if<Mov>      (&instr)) emitInstrMov(*i, rm);
            else if (auto* i  = std::get_if<DerefLoadIdx> (&instr)) emitInstrDerefLoadIdx(*i, rm);
            else if (auto* i  = std::get_if<DerefStoreIdx>(&instr)) emitInstrDerefStoreIdx(*i, rm);
            else if (auto* c  = std::get_if<CalcAddrIdx>  (&instr)) emitInstrCalcAddrIdx(*c, rm);
            else if (auto* i  = std::get_if<DerefLoad>    (&instr)) emitInstrDerefLoad(*i,  rm);
            else if (auto* i  = std::get_if<DerefStore>   (&instr)) emitInstrDerefStore(*i, rm);
            else if (auto* i  = std::get_if<CalcAddr>     (&instr)) emitInstrCalcAddr(*i, rm);
            else if (auto* i  = std::get_if<LeaLocal>     (&instr)) emitInstrLeaLocal(*i, rm);
            // BlockEnter and BlockLeave are no-ops
        }
    }

    // "Which object is the entry object" is already represented exactly once,
    // by the _start VFunc that PlnVCodeGen marks (PlnVCodeGen.cpp:744) -- derive
    // it rather than adding a second VProg flag that could disagree.
    bool isEntryObject = false;
    for (auto& f : prog.funcs)
        if (f.isEntry) { isEntryObject = true; break; }
    emitElfCrtGlue(isEntryObject);
}

void PlnX86CodeGen::emitFuncPrologue(const VFunc& func, const RegAllocResult& ra, const RegMap& rm)
{
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
}

void PlnX86CodeGen::emitInstrLeaLabel(const LeaLabel& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    if (!loc.isStack()) {
        emitLeaLabel(sizedRegName(loc.base, loc.type), i.label);
    } else {
        // Spilled dst: load address into scratch %rax then store to stack.
        out << "\tleaq " << i.label << "(%rip), %rax\n";
        out << "\tmovq %rax, " << srcOperand(loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrMovImm(const MovImm& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    if (loc.isStack()) {
        out << "\t" << movInstrForType(i.type) << " $" << i.value << ", " << srcOperand(loc) << "\n";
    } else {
        emitMovImm(sizedRegName(loc.base, i.type), i.type, i.value);
    }
}

void PlnX86CodeGen::emitInstrInitVar(const InitVar& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    if (loc.isStack()) {
        out << "\t" << movInstrForType(i.type) << " $" << i.imm << ", " << loc.stackOffset << "(%rbp)\n";
    } else {
        out << "\t" << movInstrForType(i.type) << " $" << i.imm << ", " << sizedRegName(loc.base, i.type) << "\n";
    }
}

void PlnX86CodeGen::emitInstrInitVarF(const InitVarF& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    const char* mov = movInstrForType(i.type);
    if (loc.isStack()) {
        // Load float constant into %xmm0 scratch, then store to stack slot.
        out << "\t" << mov << " " << i.label << "(%rip), %xmm0\n";
        out << "\t" << mov << " %xmm0, " << loc.stackOffset << "(%rbp)\n";
    } else {
        out << "\t" << mov << " " << i.label << "(%rip), " << loc.base << "\n";
    }
}

void PlnX86CodeGen::emitInstrCondJmp(const CondJmp& cj, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(cj.cond);
    string cond_reg;
    if (loc.isStack()) {
        out << "\tmovl " << srcOperand(loc) << ", %eax\n";
        cond_reg = "%eax";
    } else {
        cond_reg = sizedRegName(loc.base, VRegType::Int32);
    }
    out << "\ttestl " << cond_reg << ", " << cond_reg << "\n";
    out << (cj.jumpIfZero ? "\tje " : "\tjne ") << cj.label << "\n";
}

void PlnX86CodeGen::emitInstrMov(const Mov& mv, const RegMap& rm)
{
    // Copy src into dst (variable's canonical stack slot or register).
    if (!rm.count(mv.dst) || !rm.count(mv.src)) return;
    const PhysLoc& dst_loc = rm.at(mv.dst);
    const PhysLoc& src_loc = rm.at(mv.src);
    string s = srcOperand(src_loc);
    string d = srcOperand(dst_loc);
    if (s == d) return;  // same location: no-op
    if (!dst_loc.isStack()) {
        string dst_reg = sizedRegName(dst_loc.base, mv.type);
        out << "\t" << movInstrForType(mv.type) << " " << s << ", " << dst_reg << "\n";
    } else if (!src_loc.isStack()) {
        out << "\t" << movInstrForType(mv.type) << " " << s << ", " << d << "\n";
    } else {
        // Both on stack: route through scratch register.
        string scratch = isFloat(mv.type) ? "%xmm8" : sizedRegName("%rax", mv.type);
        out << "\t" << movInstrForType(mv.type) << " " << s << ", " << scratch << "\n";
        out << "\t" << movInstrForType(mv.type) << " " << scratch << ", " << d << "\n";
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

// ELF/System V target glue that a C program's crt startup objects would supply.
// Palan owns its own program entry -- it emits _start itself and the build
// manager links `ld <objs> -lc` with no crt1.o/crti.o/crtbegin.o -- so it must
// supply these itself. A non-ELF backend must not inherit this method; see the
// ticket for the deferred VProg-level VGlobal promotion if a second backend or
// a second compiler-emitted global ever appears.
void PlnX86CodeGen::emitElfCrtGlue(bool isEntryObject)
{
    // Mirrors crtbegin.o's definition for an executable (gcc crtstuff.c's
    // `void *__dso_handle = 0;`): GLOBAL, HIDDEN, 8-byte aligned, in .data, and
    // with no .size directive -- crtbegin's own symbol has size 0. glibc's
    // atexit forwards to __cxa_atexit(func, arg, __dso_handle) from
    // libc_nonshared.a, whose hidden reference has nothing to bind to
    // otherwise. Emitted whether or not this program calls atexit: the compiler
    // models no "which libc symbols will this link pull in" fact, and
    // at_quick_exit/pthread_atfork reference it too, so a use-site gate would
    // grow into a whitelist of C function names.
    if (isEntryObject) {
        emitSection(".data");
        emitGlobal("__dso_handle");
        out << "\t.hidden __dso_handle\n";
        out << "\t.type __dso_handle, @object\n";
        out << "\t.align 8\n";
        emitLabel("__dso_handle");
        out << "\t.quad 0\n";
    }

    // Every object, entry or not: ld takes the union of its inputs, so a single
    // note-less object marks the whole output's stack executable -- and since
    // binutils 2.39 also warns on stderr. Without it, a program linking
    // libc_nonshared.a gets PT_GNU_STACK RWE, and one that does not gets no
    // PT_GNU_STACK at all (kernel default: READ_IMPLIES_EXEC).
    out << "\t.section .note.GNU-stack,\"\",@progbits\n";
}

void PlnX86CodeGen::emitLeaLabel(const string& reg, const string& label)
{
    out << "\tleaq " << label << "(%rip), " << reg << "\n";
}

void PlnX86CodeGen::emitMovImm(const string& reg, VRegType type, long long value)
{
    out << "\t" << movInstrForType(type) << " $" << value << ", " << reg << "\n";
}

void PlnX86CodeGen::emitExit(int code)
{
    out << "\tmovl $" << code << ", %edi\n";
    out << "\tcall exit\n";
}
