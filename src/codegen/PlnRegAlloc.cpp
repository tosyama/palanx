#include "PlnRegAlloc.h"
#include <boost/assert.hpp>
#include <algorithm>

using namespace std;

RegAllocResult allocateRegisters(const VFunc& func, const PhysRegs& phys)
{
    // Step 1: Collect VReg metadata.
    struct VRegMeta {
        VRegType type    = VRegType::Int64;
        int      def_idx = -1;
        bool     isVar   = false;  // true if defined by InitVar (variable)
        // (call_instr_idx, arg_position) for each use as a call argument
        vector<pair<int,int>> uses;
    };
    map<VReg, VRegMeta> meta;

    vector<int> call_indices;  // instruction indices of all CallC

    for (int i = 0; i < (int)func.instrs.size(); i++) {
        auto& instr = func.instrs[i];
        if (auto* l = get_if<LeaLabel>(&instr)) {
            meta[l->dst].def_idx = i;
            meta[l->dst].type    = l->type;
        } else if (auto* m = get_if<MovImm>(&instr)) {
            meta[m->dst].def_idx = i;
            meta[m->dst].type    = m->type;
        } else if (auto* v = get_if<InitVar>(&instr)) {
            meta[v->dst].def_idx = i;
            meta[v->dst].type    = v->type;
            meta[v->dst].isVar   = true;
        } else if (auto* c = get_if<CallC>(&instr)) {
            call_indices.push_back(i);
            for (int j = 0; j < (int)c->args.size(); j++) {
                meta[c->args[j]].uses.push_back({i, j});
            }
        }
    }

    // Step 2: Assign physical registers or stack slots.
    RegMap result;
    int callee_idx  = 0;
    int stack_slots = 0;  // number of 8-byte stack slots used

    auto allocStackSlot = [&](VReg vreg, VRegType type) {
        stack_slots++;
        result[vreg] = PhysLoc{"", type, -8 * stack_slots};
    };

    for (auto& [vreg, m] : meta) {
        // InitVar VReg with no uses: store side-effect must be preserved.
        // Assign a stack slot so the emitter can still store the value.
        if (m.uses.empty()) {
            if (m.isVar) allocStackSlot(vreg, m.type);
            continue;
        }

        // Determine whether this VReg must survive across a call.
        bool needs_callee_saved = false;
        if (m.uses.size() > 1) {
            needs_callee_saved = true;
        } else {
            int use_idx = m.uses.front().first;
            for (int c_idx : call_indices) {
                if (m.def_idx < c_idx && c_idx < use_idx) {
                    needs_callee_saved = true;
                    break;
                }
            }
        }

        if (needs_callee_saved) {
            if (callee_idx < (int)phys.calleeSaved.size()) {
                result[vreg] = PhysLoc{phys.calleeSaved[callee_idx++], m.type};
            } else {
                // Callee-saved registers exhausted: spill to stack.
                allocStackSlot(vreg, m.type);
            }
        } else {
            // Assign directly to the required argument register.
            int arg_pos = m.uses.front().second;
            BOOST_ASSERT(arg_pos < (int)phys.intArgs.size());
            result[vreg] = PhysLoc{phys.intArgs[arg_pos], m.type};
        }
    }

    // Align frame size to 16 bytes.
    // After pushq %rbp the stack is offset by 8, so we need frameSize ≡ 8 (mod 16)
    // to keep rsp 16-byte aligned before each call.
    int frameSize = stack_slots * 8;
    if (frameSize > 0 && frameSize % 16 == 0) frameSize += 8;

    return {result, frameSize};
}
