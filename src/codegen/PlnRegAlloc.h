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

// Physical register location: either a physical register or a stack slot.
//   base non-empty → register (64-bit name, e.g. "%rdi")
//   base empty     → stack slot; stackOffset is the %rbp-relative offset (e.g. -8)
struct PhysLoc {
    string   base;           // 64-bit base name; empty if stack slot
    VRegType type;
    int      stackOffset = 0;  // valid when base is empty
    bool isStack() const { return base.empty(); }
};

using RegMap = map<VReg, PhysLoc>;

struct RegAllocResult {
    // Parameter remapped from its argument register to a callee-saved register or stack slot.
    // X86CodeGen emits a move from srcArgReg to dst at function entry.
    struct ParamCopy {
        string   srcArgReg;  // 64-bit argument register base name (e.g. "%rsi")
        PhysLoc  dst;
        VRegType type;
    };

    RegMap regMap;
    int    frameSize;          // bytes to reserve on stack (0 if no spills/callee-saves needed)
    vector<string>    usedCalleeSaved;  // callee-saved regs used, in allocation order
    vector<ParamCopy> paramCopies;      // params moved from arg regs due to call-crossing
};

// Physical register lists (architecture-specific, passed in by caller)
struct PhysRegs {
    vector<string> intArgs;     // integer/pointer arg regs: "%rdi", "%rsi", ...
    vector<string> floatArgs;   // floating-point arg regs: "%xmm0", "%xmm1", ...
    vector<string> calleeSaved; // callee-saved regs: "%rbx", "%r12", ...
};

// Live-range-based register allocator.
// Assigns each VReg to a physical register or stack slot:
//   - VRegs used only within one call segment (no intervening call) →
//     assigned directly to the required argument register.
//   - VRegs live across a call → callee-saved register.
//   - When callee-saved registers are exhausted → spill to stack slot.
//   - InitVar VRegs with no uses → stack slot (preserves the store side-effect).
//
// Returns RegAllocResult containing the RegMap and the required frame size.
RegAllocResult allocateRegisters(const VFunc& func, const PhysRegs& phys);
