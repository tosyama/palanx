/// Abstract code generator interface.
///
/// @file codegen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include "vprog.h"

using std::string;

class CodeGen {
public:
    virtual ~CodeGen() = default;
    virtual void emit(const VProg& prog) = 0;
};
