/// Palan common definitions.
///
/// @file PlnDefs.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>

#define PALAN_VERSION "0.2.4"
#define PALAN_TARGET "x86_64-linux-gnu"

inline bool isSupportedTarget(const std::string& target)
{
	return target == PALAN_TARGET;
}

// Stem of the module build-mgr generates the __pln_* allocators into. palan-sa
// names that module in the symbols of the calls it makes to them.
#define PALAN_ALLOCATORS_MODULE "__allocators"
