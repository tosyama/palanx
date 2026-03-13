/// Virtual instruction set (IR) for palan-codegen.
///
/// @file vprog.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <vector>
#include <variant>

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

struct LeaLabel { VReg dst; VRegType type; string label; };  // dst = address of label
struct CallC    { string name; int argCount; };
struct ExitCode { int code; };

using VInstr = std::variant<LeaLabel, CallC, ExitCode>;

// -------- Program structure --------

struct VStringLiteral {
    string label;
    string value;
};

struct VFunc {
    string name;
    vector<VInstr> instrs;
};

struct VProg {
    vector<VStringLiteral> data;   // .rodata
    vector<VFunc>          funcs;  // .text
};
