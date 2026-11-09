/// The text a string literal's value travels as through the JSON files.
///
/// @file PlnStrLit.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>

// Length of the valid UTF-8 sequence of two or more bytes at s[i], or 0.
inline size_t utf8SeqAt(const std::string& s, size_t i)
{
	auto b = [&](size_t k) { return static_cast<unsigned char>(s[k]); };
	unsigned char c = b(i);
	size_t n = c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
	if (n == 0 || i + n > s.size()) return 0;
	for (size_t k = 1; k < n; ++k)
		if (b(i + k) < 0x80 || b(i + k) > 0xBF) return 0;
	unsigned char c1 = b(i + 1);
	if ((c == 0xE0 && c1 < 0xA0) || (c == 0xED && c1 > 0x9F)
		|| (c == 0xF0 && c1 < 0x90) || (c == 0xF4 && c1 > 0x8F)) return 0;
	return n;
}

// JSON holds only valid UTF-8, so every byte that would break it, or be hard to read, is escaped.
inline std::string strLitText(const std::string& bytes)
{
	static const char* hex = "0123456789ABCDEF";
	std::string t;
	for (size_t i = 0; i < bytes.size(); ++i) {
		unsigned char c = bytes[i];
		switch (c) {
			case '"': t += "\\\""; continue;
			case '\\': t += "\\\\"; continue;
			case '\n': t += "\\n"; continue;
			case '\t': t += "\\t"; continue;
			case '\r': t += "\\r"; continue;
		}
		if (c >= 0x20 && c < 0x7F) { t += c; continue; }
		if (size_t n = c >= 0x80 ? utf8SeqAt(bytes, i) : 0) {
			t.append(bytes, i, n);
			i += n - 1;
			continue;
		}
		t += "\\x";
		t += hex[c >> 4];
		t += hex[c & 15];
	}
	return t;
}

// The inverse of strLitText.
inline std::string strLitBytes(const std::string& text)
{
	auto hexv = [](char h) { return h <= '9' ? h - '0' : h - 'A' + 10; };
	std::string b;
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] != '\\') { b += text[i]; continue; }
		char e = text[++i];
		switch (e) {
			case 'n': b += '\n'; break;
			case 't': b += '\t'; break;
			case 'r': b += '\r'; break;
			case 'x': b += static_cast<char>(hexv(text[i + 1]) * 16 + hexv(text[i + 2])); i += 2; break;
			default: b += e;
		}
	}
	return b;
}
