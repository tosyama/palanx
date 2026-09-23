#include <gtest/gtest.h>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnDeserialize.h"
#include "PlnVCodeGen.h"

using json = nlohmann::json;

// Verifies the func-type:"syscall" call node's path through PlnDeserialize
// (sa.json -> SysCallExpr) and PlnVCodeGen (SysCallExpr -> CallSys VInstr),
// the codegen layers IT-2026-09-19-3205 adds. sa.json shapes below are
// copied from real `palan-sa` output (see test/testdata/sa/189_syscall_call.pa),
// not hand-guessed.

// Value context: init expression of a var-decl. hasRet is true, so a single
// dst vreg is allocated and retTypes carries the call's value-type.
TEST(syscall_lower, value_context) {
    json sa = json::parse(R"({
        "functions": [
            {"name":"main", "func-type":"palan", "parameters":[],
             "ret-type":{"type-kind":"prim","type-name":"int64"},
             "body":[
                {"stmt-type":"var-decl", "vars":[
                    {"name":"n", "var-type":{"type-kind":"prim","type-name":"int64"},
                     "init":{"expr-type":"call", "func-type":"syscall", "name":"write",
                             "syscall-number":1,
                             "args":[{"expr-type":"lit-int","value":"1",
                                      "value-type":{"type-kind":"prim","type-name":"int32"}}],
                             "value-type":{"type-kind":"prim","type-name":"int64"}}}
                ]},
                {"stmt-type":"return", "values":[
                    {"expr-type":"id","name":"n","value-type":{"type-kind":"prim","type-name":"int64"}}
                ]}
             ]}
        ]
    })");

    Module mod = deserialize(sa);
    VProg prog = PlnVCodeGen().generate(mod, true);

    const VFunc* mainFunc = nullptr;
    for (auto& f : prog.funcs) if (f.name == "main") mainFunc = &f;
    ASSERT_NE(mainFunc, nullptr);
    const CallSys* call = nullptr;
    for (auto& instr : mainFunc->instrs)
        if (auto* c = std::get_if<CallSys>(&instr)) call = c;
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->num, 1);
    ASSERT_EQ(call->args.size(), 1u);
    ASSERT_EQ(call->dsts.size(), 1u);
    ASSERT_EQ(call->retTypes.size(), 1u);
    EXPECT_EQ(call->retTypes[0], VRegType::Int64);
}

// Statement context: the call is a bare ExprStmt. Even though the callee has
// a return value (value-type present, matching real sa.json for a
// statement-context call), the return is discarded -- 0 dsts, 0 retTypes.
TEST(syscall_lower, stmt_context_discards_return) {
    json sa = json::parse(R"({
        "functions": [
            {"name":"main", "func-type":"palan", "parameters":[],
             "body":[
                {"stmt-type":"expr", "body":
                    {"expr-type":"call", "func-type":"syscall", "name":"write",
                     "syscall-number":1,
                     "args":[{"expr-type":"lit-int","value":"1",
                              "value-type":{"type-kind":"prim","type-name":"int32"}}],
                     "value-type":{"type-kind":"prim","type-name":"int64"}}}
             ]}
        ]
    })");

    Module mod = deserialize(sa);
    VProg prog = PlnVCodeGen().generate(mod, true);

    const VFunc* mainFunc = nullptr;
    for (auto& f : prog.funcs) if (f.name == "main") mainFunc = &f;
    ASSERT_NE(mainFunc, nullptr);
    const CallSys* call = nullptr;
    for (auto& instr : mainFunc->instrs)
        if (auto* c = std::get_if<CallSys>(&instr)) call = c;
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->num, 1);
    EXPECT_EQ(call->dsts.size(), 0u);
    EXPECT_EQ(call->retTypes.size(), 0u);
}

// Regression guard: an identically-shaped func-type:"palan" call must still
// deserialize to CallPln, not CallSys -- the "c"/"syscall"/else split in
// PlnDeserialize.cpp must not have broken the pre-existing fallthrough.
TEST(syscall_lower, palan_call_unaffected) {
    json sa = json::parse(R"({
        "functions": [
            {"name":"main", "func-type":"palan", "parameters":[],
             "body":[
                {"stmt-type":"expr", "body":
                    {"expr-type":"call", "func-type":"palan", "name":"helper", "args":[]}}
             ]}
        ]
    })");

    Module mod = deserialize(sa);
    VProg prog = PlnVCodeGen().generate(mod, true);

    const VFunc* mainFunc = nullptr;
    for (auto& f : prog.funcs) if (f.name == "main") mainFunc = &f;
    ASSERT_NE(mainFunc, nullptr);
    bool foundCallPln = false, foundCallSys = false;
    for (auto& instr : mainFunc->instrs) {
        if (std::get_if<CallPln>(&instr)) foundCallPln = true;
        if (std::get_if<CallSys>(&instr)) foundCallSys = true;
    }
    EXPECT_TRUE(foundCallPln);
    EXPECT_FALSE(foundCallSys);
}
