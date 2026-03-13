/// x86-64 AT&T assembly code generator.
///
/// @file x86codegen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <ostream>
#include "codegen.h"
#include "regalloc.h"

class X86CodeGen : public CodeGen {
    std::ostream& out;

    void emitSection(const string& name);
    void emitGlobal(const string& name);
    void emitLabel(const string& name);
    void emitStringLiteral(const string& label, const string& value);
    void emitLeaLabel(const string& reg, const string& label);
    void emitCallC(const string& name, int argCount);
    void emitExit(int code);

public:
    explicit X86CodeGen(std::ostream& out) : out(out) {}

    void emit(const VProg& prog) override;
};
