/// Placement of a C bit-field, shared by c2ast's sizeof and SA's struct layout.
///
/// @file PlnCBitField.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <cstdint>

// Returns the bit offset GCC on x86-64 gives a bit-field of `width` bits whose
// declared type aligns to `typeAlign` bytes, the previous member ending at
// `bitPos`. A bit-field never straddles a unit of its type unless packed; a
// zero-width one moves to the next unit even in a packed struct.
inline int64_t placeCBitField(int64_t bitPos, int64_t width, int64_t typeAlign, bool packed)
{
	int64_t unit = typeAlign * 8;
	auto next = [&] { return (bitPos + unit - 1) / unit * unit; };
	if (width == 0) return next();
	if (!packed && bitPos / unit != (bitPos + width - 1) / unit) return next();
	return bitPos;
}
