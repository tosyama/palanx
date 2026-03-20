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
    Float32,
    Float64,
};

// -------- Virtual instructions --------

struct LeaLabel { VReg dst; VRegType type; string label; };       // dst = address of label
struct MovImm   { VReg dst; VRegType type; long long value; };    // dst = immediate integer
struct InitVar  { VReg dst; VRegType type; long long imm; };      // variable init: dst_vreg = imm
struct Add      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs + rhs
struct Convert  { VReg dst; VReg src; VRegType from; VRegType to; }; // dst = (to)src
struct CallC    { string name; vector<VReg> args; VReg dst = -1; VRegType retType = VRegType::Int64; };
struct CallPln  { string name; vector<VReg> args; vector<VReg> dsts; vector<VRegType> retTypes; };
struct RetPln   { vector<VReg> rets; vector<VRegType> types; };
struct ExitCode { int code; };
struct BlockEnter {};
struct BlockLeave { vector<VReg> expiredVars; };

using VInstr = std::variant<LeaLabel, MovImm, InitVar, Add, Convert,
                             CallC, CallPln, RetPln, ExitCode,
                             BlockEnter, BlockLeave>;

// -------- Program structure --------

struct VStringLiteral {
    string label;
    string value;
};

struct VFunc {
    string         name;
    vector<VInstr> instrs;
    bool           isEntry = false;  // true only for _start
    vector<std::pair<VReg, VRegType>> params;  // (vreg, type) in arg order
};

struct VProg {
    vector<VStringLiteral> data;   // .rodata
    vector<VFunc>          funcs;  // .text
};
