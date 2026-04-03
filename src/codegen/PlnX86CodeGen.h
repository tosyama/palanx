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

    void emitSection(const string& name);
    void emitGlobal(const string& name);
    void emitLabel(const string& name);
    void emitStringLiteral(const string& label, const string& value);
    void emitLeaLabel(const string& reg, const string& label);
    void emitMovImm(const string& reg, VRegType type, long long value);
    void emitConvert(const string& dst, const PhysLoc& src, VRegType from, VRegType to);
    void emitCallC(const string& name, int nFloatArgs = 0);
    void emitExit(int code);

public:
    explicit PlnX86CodeGen(std::ostream& out) : out(out) {}

    void emit(const VProg& prog) override;
};
