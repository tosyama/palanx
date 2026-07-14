/// Abstract code generator interface.
///
/// @file PlnCodeGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include "PlnVProg.h"
#include "PlnRegAlloc.h"

using std::string;

class PlnCodeGen {
public:
    virtual ~PlnCodeGen() = default;
    virtual void emit(const VProg& prog, const vector<RegAllocResult>& allocs) = 0;
};
