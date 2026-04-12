/// Virtual instruction set (IR) for palan-codegen.
///
/// @file PlnVProg.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <vector>
#include <variant>
#include <utility>

using std::string;
using std::vector;

using VReg = int;

// Data size / type of a virtual register value
enum class VRegType {
    Ptr64,    // pointer (64-bit)
    Int8,
    Int16,
    Int32,
    Int64,
    Uint8,
    Uint16,
    Uint32,
    Uint64,
    Float32,
    Float64,
};

// -------- Virtual instructions --------

struct LeaLabel  { VReg dst; VRegType type; string label; };       // dst = address of label
struct MovImm    { VReg dst; VRegType type; long long value; };    // dst = immediate integer
struct InitVar   { VReg dst; VRegType type; long long imm; };      // variable init: dst_vreg = imm
struct InitVarF  { VReg dst; VRegType type; string label; };       // variable init: dst_vreg = float at label
struct Add      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs + rhs
struct Sub      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs - rhs
struct Mul      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs * rhs
struct Div      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs / rhs
struct Mod      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs % rhs
struct Neg      { VReg dst; VReg src; VRegType type; };            // dst = -src
struct Cmp      { VReg dst; string op; VReg lhs; VReg rhs; VRegType type; }; // dst (int32) = (lhs op rhs) ? 1 : 0
struct Convert  { VReg dst; VReg src; VRegType from; VRegType to; }; // dst = (to)src
struct CallC    { string name; vector<VReg> args; VReg dst = -1; VRegType retType = VRegType::Int64; };
struct CallPln  { string name; vector<VReg> args; vector<VReg> dsts; vector<VRegType> retTypes; };
struct RetPln   { vector<VReg> rets; vector<VRegType> types; };
struct ExitCode { int code; };
struct BlockEnter {};
struct BlockLeave { vector<VReg> expiredVars; };
struct Label      { string name; };                              // name:
struct Jmp        { string label; };                             // jmp label
struct CondJmp    { string label; VReg cond; bool jumpIfZero; }; // testl+je/jne
struct Mov        { VReg dst; VReg src; VRegType type; };        // dst = src (variable update)

using VInstr = std::variant<LeaLabel, MovImm, InitVar, InitVarF, Add, Sub, Mul, Div, Mod, Neg, Cmp, Convert,
                             CallC, CallPln, RetPln, ExitCode,
                             BlockEnter, BlockLeave,
                             Label, Jmp, CondJmp, Mov>;

// -------- Program structure --------

struct VStringLiteral {
    string label;
    string value;
};

struct VFloatLiteral {
    string   label;
    string   value;   // decimal string e.g. "3.14"
    VRegType type;    // Float32 or Float64
};

struct VFunc {
    string         name;
    vector<VInstr> instrs;
    bool           isEntry  = false;  // true only for _start
    bool           isExport = false;  // true for export func
    vector<std::pair<VReg, VRegType>> params;  // (vreg, type) in arg order
};

struct VProg {
    vector<VStringLiteral> data;       // .rodata string literals
    vector<VFloatLiteral>  floatData;  // .rodata float literals
    vector<VFunc>          funcs;      // .text
    bool needsF32Neg = false;          // true if any Neg{Float32} instruction exists
    bool needsF64Neg = false;          // true if any Neg{Float64} instruction exists
};
