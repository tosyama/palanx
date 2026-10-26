/// palan build manager test helpers
///
/// @file buildMgrTestUtil.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
#include <utility>

std::pair<int,int> parseMtraceLog(const std::string& traceFile);
std::string runWithCObj(const std::string& base);
void expectFrameMtrace(const std::string& base, const std::string& expected);
void expectBalancedMtrace(const std::string& traceFile, int expectedAllocs);
void expectMtraceRun(const std::string& name, const std::string& expected, int expectedAllocs);
