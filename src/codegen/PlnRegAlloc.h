/// Virtual register allocator.
///
/// @file PlnRegAlloc.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <map>
#include <string>
#include <vector>
#include "PlnVProg.h"

using std::map;
using std::string;
using std::vector;

// Physical register location: base name (64-bit form) + data type.
// The emitter derives the sized register name from (base, type):
//   e.g. base="%rdi", type=Int32 -> "%edi"
struct PhysLoc {
    string   base;  // 64-bit base name: "%rdi", "%xmm0", etc.
    VRegType type;
};

using RegMap = map<VReg, PhysLoc>;

// Physical register lists (architecture-specific, passed in by caller)
struct PhysRegs {
    vector<string> intArgs;     // integer/pointer arg regs: "%rdi", "%rsi", ...
    vector<string> floatArgs;   // floating-point arg regs: "%xmm0", "%xmm1", ...
    vector<string> calleeSaved; // callee-saved regs: "%rbx", "%r12", ...
};

// Live-range-based register allocator.
// Assigns each VReg to a physical register:
//   - VRegs used only within one call segment (def to use, no intervening call)
//     are assigned directly to the required argument register.
//   - VRegs live across a call are assigned to a callee-saved register;
//     the x86 emitter inserts moves to argument registers at each call site.
//   - Spill to stack only when physical registers are exhausted (not-impl yet).
RegMap allocateRegisters(const VFunc& func, const PhysRegs& phys);
