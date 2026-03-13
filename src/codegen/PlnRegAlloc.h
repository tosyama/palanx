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
    vector<string> intArgs;    // integer/pointer arg regs: "%rdi", "%rsi", ...
    vector<string> floatArgs;  // floating-point arg regs: "%xmm0", "%xmm1", ...
};

// Trivial allocator: assigns VRegs to physical registers based on instruction kind.
// Register kind is inferred from the defining instruction:
//   LeaLabel -> pointer -> intArgs[intArgIdx++]
//   (future) LoadDbl -> float -> floatArgs[floatArgIdx++]
// Both counters reset after each CallC.
//
// Future: replace this function with a proper live-range-based allocator.
RegMap allocateRegisters(const VFunc& func, const PhysRegs& phys);
