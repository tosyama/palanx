#include <gtest/gtest.h>
#include "PlnRegAlloc.h"

// Minimal PhysRegs used across tests: 2 int-arg regs, 1 callee-saved reg.
static const PhysRegs testPhys = {
    { "%rdi", "%rsi" },   // intArgs
    {},                    // floatArgs
    { "%rbx" }             // calleeSaved
};

// -------- Stack slot sizing --------

// int64 variable → 8-byte slot at -8(%rbp), frameSize 8
TEST(regalloc, stack_int64) {
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(InitVar{0, VRegType::Int64, 0});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).stackOffset, -8);
    EXPECT_EQ(r.regMap.at(0).type,        VRegType::Int64);
    EXPECT_EQ(r.frameSize, 8);
}

// int32 variable → 4-byte slot at -4(%rbp), frameSize 8 (alignUp(4,8))
TEST(regalloc, stack_int32) {
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(InitVar{0, VRegType::Int32, 0});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).stackOffset, -4);
    EXPECT_EQ(r.regMap.at(0).type,        VRegType::Int32);
    EXPECT_EQ(r.frameSize, 8);
}

// int32 then int64 → int64 aligned to 8-byte boundary; padding between them
TEST(regalloc, stack_mixed_alignment) {
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(InitVar{0, VRegType::Int32, 0});  // offset -4
    func.instrs.push_back(InitVar{1, VRegType::Int64, 0});  // alignUp(4,8)=8, offset -16

    auto r = allocateRegisters(func, testPhys);

    EXPECT_EQ(r.regMap.at(0).stackOffset, -4);
    EXPECT_EQ(r.regMap.at(1).stackOffset, -16);
    EXPECT_EQ(r.frameSize, 24);  // alignUp(16,8)=16, 16%16==0 → +8 → 24
}

// Two int64 variables → frameSize bumped to keep 16-byte call alignment
TEST(regalloc, stack_frame_alignment) {
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(InitVar{0, VRegType::Int64, 0});  // offset -8
    func.instrs.push_back(InitVar{1, VRegType::Int64, 0});  // offset -16

    auto r = allocateRegisters(func, testPhys);

    EXPECT_EQ(r.regMap.at(0).stackOffset, -8);
    EXPECT_EQ(r.regMap.at(1).stackOffset, -16);
    EXPECT_EQ(r.frameSize, 24);  // 16%16==0 → +8 → 24
}

// -------- Register assignment --------

// Value used only as arg to a single call → assigned to that arg register
TEST(regalloc, arg_register) {
    VFunc func;
    func.instrs.push_back(MovImm{0, VRegType::Int64, 42});
    func.instrs.push_back(CallC{"foo", {0}});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_FALSE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).base, "%rdi");
    EXPECT_EQ(r.frameSize, 0);
}

// Value used as arg by two calls → needs callee-saved register
TEST(regalloc, callee_saved_two_call_uses) {
    VFunc func;
    func.instrs.push_back(MovImm{0, VRegType::Ptr64, 0});  // r0 = label
    func.instrs.push_back(CallC{"puts",   {0}});            // first use
    func.instrs.push_back(CallC{"puts",   {0}});            // second use → callee-saved

    auto r = allocateRegisters(func, testPhys);

    ASSERT_FALSE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).base, "%rbx");
}

// -------- Bug-prone cases --------

// Dead vreg (defined but never used) → not allocated; absent from RegMap
TEST(regalloc, dead_vreg_not_allocated) {
    VFunc func;
    func.instrs.push_back(MovImm{0, VRegType::Int64, 42});  // never used

    auto r = allocateRegisters(func, testPhys);

    EXPECT_EQ(r.regMap.count(0), 0u);
    EXPECT_EQ(r.frameSize, 0);
}

// callee-saved registers exhausted → second vreg spills to stack with correct type size
TEST(regalloc, callee_saved_spill_to_stack) {
    // testPhys has only 1 callee-saved (%rbx); second vreg needing callee-saved spills.
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(MovImm{0, VRegType::Ptr64, 0});   // r0 def at 0
    func.instrs.push_back(MovImm{1, VRegType::Int32, 42});  // r1 def at 1
    func.instrs.push_back(CallC{"foo", {}});                 // idx 2: intervening call
    func.instrs.push_back(CallC{"bar", {0}});                // r0 used at 3 → callee-saved → %rbx
    func.instrs.push_back(CallC{"baz", {1}});                // r1 used at 4 → spills (Int32 stack)

    auto r = allocateRegisters(func, testPhys);

    // r0 (processed first, VReg 0 < 1) claims the only callee-saved register
    ASSERT_FALSE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).base, "%rbx");
    // r1 spills; must use 4-byte slot (Int32), not 8-byte
    ASSERT_TRUE(r.regMap.at(1).isStack());
    EXPECT_EQ(r.regMap.at(1).stackOffset, -4);
    EXPECT_EQ(r.regMap.at(1).type, VRegType::Int32);
    EXPECT_EQ(r.frameSize, 8);  // alignUp(4,8)=8
}

// Convert src: has last_any_use but no call_uses → allocCalleeSavedOrStack path
TEST(regalloc, convert_src_uses_callee_saved) {
    // r0 is used only as Convert source, not directly by a call.
    // It must be in a register (callee-saved) because it's needed by the Convert instruction.
    VFunc func;
    func.instrs.push_back(MovImm{0, VRegType::Int32,  10});              // r0 def at 0
    func.instrs.push_back(Convert{1, 0, VRegType::Int32, VRegType::Int64}); // r1 = (int64)r0; r0.last_any_use=1
    func.instrs.push_back(CallC{"foo", {1}});                             // r1 used by call

    auto r = allocateRegisters(func, testPhys);

    // r0: no call_uses, last_any_use>=0, !isVar → allocCalleeSavedOrStack → %rbx
    ASSERT_FALSE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).base, "%rbx");
    // r1: call_use, no intervening call → arg register %rdi
    ASSERT_FALSE(r.regMap.at(1).isStack());
    EXPECT_EQ(r.regMap.at(1).base, "%rdi");
    // k=1 callee-saved used → save area = 8 bytes → frameSize = alignUp(8,16) = 16
    EXPECT_EQ(r.frameSize, 16);
}

// -------- Efficiency cases --------

// InitVar with a direct call use bypasses isVar→stack rule → gets arg register (no stack needed)
// InitVar (isVar) always goes to stack regardless of call uses — callers load from stack
TEST(regalloc, initvar_direct_call_use_always_stack) {
    VFunc func;
    func.instrs.push_back(InitVar{0, VRegType::Int64, 10});
    func.instrs.push_back(CallC{"foo", {0}});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    // Stack layout: 8-byte slot for r0 → frameSize = alignUp(8,16) = 16
    EXPECT_EQ(r.frameSize, 16);
}

// InitVar with intervening call → still on stack (isVar always stack-allocated)
TEST(regalloc, initvar_with_intervening_call_always_stack) {
    VFunc func;
    func.instrs.push_back(InitVar{0, VRegType::Int64, 10});  // r0 def at 0
    func.instrs.push_back(CallC{"foo", {}});                  // idx 1: intervening
    func.instrs.push_back(CallC{"bar", {0}});                 // r0 used at 2

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.frameSize, 16);
}

// Add result used directly by call → gets arg register; Add operands go to stack
TEST(regalloc, add_result_as_arg_register) {
    VFunc func;
    func.instrs.push_back(InitVar{0, VRegType::Int64, 10});  // r0: stack (isVar, no call_use)
    func.instrs.push_back(InitVar{1, VRegType::Int64, 20});  // r1: stack
    func.instrs.push_back(Add{2, 0, 1, VRegType::Int64});    // r2 = r0+r1
    func.instrs.push_back(CallC{"foo", {2}});                 // r2 → %rdi

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    ASSERT_TRUE(r.regMap.at(1).isStack());
    ASSERT_FALSE(r.regMap.at(2).isStack());
    EXPECT_EQ(r.regMap.at(2).base, "%rdi");
}

// Convert result used directly by call → arg register; int32 source var → 4-byte stack slot
TEST(regalloc, convert_result_as_arg_register) {
    VFunc func;
    func.instrs.push_back(InitVar{0, VRegType::Int32, 10});              // r0: int32 stack var
    func.instrs.push_back(Convert{1, 0, VRegType::Int32, VRegType::Int64}); // r1 = (int64)r0
    func.instrs.push_back(CallC{"foo", {1}});                             // r1 → %rdi

    auto r = allocateRegisters(func, testPhys);

    // r0: isVar, no call_uses → stack; int32 → 4-byte slot
    ASSERT_TRUE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).stackOffset, -4);
    EXPECT_EQ(r.regMap.at(0).type, VRegType::Int32);
    // r1: Convert result, direct call use → arg register
    ASSERT_FALSE(r.regMap.at(1).isStack());
    EXPECT_EQ(r.regMap.at(1).base, "%rdi");
}

// -------- Block scope slot reuse --------

// Sequential blocks with same-size vars → second block reuses first block's stack slot
TEST(regalloc, block_reuse_same_size) {
    VFunc func;
    func.isEntry = true;
    // block 1: v0 (Int64) allocated, then freed
    func.instrs.push_back(BlockEnter{});
    func.instrs.push_back(InitVar{0, VRegType::Int64, 0});
    func.instrs.push_back(BlockLeave{{0}});
    // block 2: v1 (Int64) should reuse v0's slot
    func.instrs.push_back(BlockEnter{});
    func.instrs.push_back(InitVar{1, VRegType::Int64, 0});
    func.instrs.push_back(BlockLeave{{1}});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    ASSERT_TRUE(r.regMap.at(1).isStack());
    EXPECT_EQ(r.regMap.at(0).stackOffset, r.regMap.at(1).stackOffset);
    // Only one slot allocated — frameSize should reflect a single Int64 slot
    EXPECT_EQ(r.frameSize, 8);
}

// Sequential blocks with different-size vars → slots must NOT be shared
TEST(regalloc, block_reuse_size_mismatch) {
    VFunc func;
    func.isEntry = true;
    // block 1: v0 (Int32)
    func.instrs.push_back(BlockEnter{});
    func.instrs.push_back(InitVar{0, VRegType::Int32, 0});
    func.instrs.push_back(BlockLeave{{0}});
    // block 2: v1 (Int64) — different size, must not reuse v0's slot
    func.instrs.push_back(BlockEnter{});
    func.instrs.push_back(InitVar{1, VRegType::Int64, 0});
    func.instrs.push_back(BlockLeave{{1}});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    ASSERT_TRUE(r.regMap.at(1).isStack());
    EXPECT_NE(r.regMap.at(0).stackOffset, r.regMap.at(1).stackOffset);
}

// Value defined before a call and used after it → needs callee-saved register
TEST(regalloc, callee_saved_across_call) {
    VFunc func;
    func.instrs.push_back(MovImm{0, VRegType::Ptr64,  0});  // r0: def at idx 0
    func.instrs.push_back(CallC{"puts", {}});                // call at idx 1 (between def and use)
    func.instrs.push_back(MovImm{1, VRegType::Int64, 42});  // r1: 42
    func.instrs.push_back(CallC{"printf", {0, 1}});         // r0 used at idx 3

    auto r = allocateRegisters(func, testPhys);

    ASSERT_FALSE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).base, "%rbx");
    // r1: single use, no intervening call between def(idx 2) and use(idx 3)
    ASSERT_FALSE(r.regMap.at(1).isStack());
    EXPECT_EQ(r.regMap.at(1).base, "%rsi");
}

// flo64 variable → 8-byte slot at -8(%rbp)
TEST(regalloc, stack_float64) {
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(InitVarF{0, VRegType::Float64, ".LF0"});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).stackOffset, -8);
    EXPECT_EQ(r.regMap.at(0).type,        VRegType::Float64);
    EXPECT_EQ(r.frameSize, 8);
}

// flo32 variable → 4-byte slot at -4(%rbp)
TEST(regalloc, stack_float32) {
    VFunc func;
    func.isEntry = true;
    func.instrs.push_back(InitVarF{0, VRegType::Float32, ".LF0"});

    auto r = allocateRegisters(func, testPhys);

    ASSERT_TRUE(r.regMap.at(0).isStack());
    EXPECT_EQ(r.regMap.at(0).stackOffset, -4);
    EXPECT_EQ(r.regMap.at(0).type,        VRegType::Float32);
    EXPECT_EQ(r.frameSize, 8);
}
