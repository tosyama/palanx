/// x86-64 AT&T assembly code generator.
///
/// @file PlnX86CodeGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <ostream>
#include "PlnCodeGen.h"
#include "PlnRegAlloc.h"

class PlnX86CodeGen : public PlnCodeGen {
    std::ostream& out;

    // Low-level emit helpers
    void emitSection(const string& name);
    void emitGlobal(const string& name);
    void emitLabel(const string& name);
    void emitStringLiteral(const string& label, const string& value);
    void emitLeaLabel(const string& reg, const string& label);
    void emitMovImm(const string& reg, VRegType type, long long value);
    void emitConvert(const string& dst, const PhysLoc& src, VRegType from, VRegType to);
    void emitCallC(const string& name, int nFloatArgs = 0);
    void emitExit(int code);

    // Per-function prologue
    void emitFuncPrologue(const VFunc& func, const RegAllocResult& ra, const RegMap& rm);

    // Shared helper: emit a binary reg-reg arithmetic instruction (Add/Sub/Mul)
    void emitBinArith(const char* op, VReg dst, VReg lhs, VReg rhs, VRegType type, const RegMap& rm);

    // Per-instruction emit helpers
    void emitInstrLeaLabel(const LeaLabel& i, const RegMap& rm);
    void emitInstrMovImm(const MovImm& i, const RegMap& rm);
    void emitInstrInitVar(const InitVar& i, const RegMap& rm);
    void emitInstrInitVarF(const InitVarF& i, const RegMap& rm);
    void emitInstrDiv(const Div& i, const RegMap& rm);
    void emitInstrMod(const Mod& i, const RegMap& rm);
    void emitInstrNeg(const Neg& i, const RegMap& rm);
    void emitInstrCmp(const Cmp& i, const RegMap& rm);
    void emitInstrConvert(const Convert& i, const RegMap& rm);
    void emitInstrCallC(const CallC& i, const RegMap& rm);
    void emitInstrCallPln(const CallPln& i, const RegMap& rm);
    void emitInstrRetPln(const RetPln& i, const RegMap& rm, const vector<string>& usedCalleeSaved);
    void emitInstrCondJmp(const CondJmp& i, const RegMap& rm);
    void emitInstrMov(const Mov& i, const RegMap& rm);
    void emitInstrDerefLoadIdx(const DerefLoadIdx& dl, const RegMap& rm);
    void emitInstrDerefStoreIdx(const DerefStoreIdx& ds, const RegMap& rm);

public:
    explicit PlnX86CodeGen(std::ostream& out) : out(out) {}

    void emit(const VProg& prog) override;
};
