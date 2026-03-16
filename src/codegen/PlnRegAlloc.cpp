#include "PlnRegAlloc.h"
#include <boost/assert.hpp>
#include <algorithm>

using namespace std;

RegAllocResult allocateRegisters(const VFunc& func, const PhysRegs& phys)
{
    // Step 1: Collect VReg metadata.
    struct VRegMeta {
        VRegType type         = VRegType::Int64;
        int      def_idx      = -1;
        bool     isVar        = false;   // true if defined by InitVar (variable)
        int      last_any_use = -1;      // last use by Add (non-call) instructions
        vector<pair<int,int>> call_uses; // (call_instr_idx, arg_position)
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
        } else if (auto* c = get_if<Convert>(&instr)) {
            meta[c->dst].def_idx = i;
            meta[c->dst].type    = c->to;
            meta[c->src].last_any_use = max(meta[c->src].last_any_use, i);
        } else if (auto* a = get_if<Add>(&instr)) {
            meta[a->dst].def_idx = i;
            meta[a->dst].type    = a->type;
            meta[a->lhs].last_any_use = max(meta[a->lhs].last_any_use, i);
            meta[a->rhs].last_any_use = max(meta[a->rhs].last_any_use, i);
        } else if (auto* c = get_if<CallC>(&instr)) {
            call_indices.push_back(i);
            for (int j = 0; j < (int)c->args.size(); j++) {
                meta[c->args[j]].call_uses.push_back({i, j});
            }
        }
    }

    // Step 2: Assign physical registers or stack slots.
    RegMap result;
    int callee_idx  = 0;
    int stack_bytes = 0;  // bytes consumed below %rbp so far

    static auto sizeOfType = [](VRegType type) -> int {
        switch (type) {
            case VRegType::Int8:  return 1;
            case VRegType::Int16: return 2;
            case VRegType::Int32: return 4;
            default:              return 8;  // Int64, Ptr64
        }
    };

    static auto alignUp = [](int v, int align) {
        return (v + align - 1) / align * align;
    };

    auto allocStackSlot = [&](VReg vreg, VRegType type) {
        int size    = sizeOfType(type);
        stack_bytes = alignUp(stack_bytes, size) + size;
        result[vreg] = PhysLoc{"", type, -stack_bytes};
    };

    auto allocCalleeSavedOrStack = [&](VReg vreg, VRegType type) {
        if (callee_idx < (int)phys.calleeSaved.size()) {
            result[vreg] = PhysLoc{phys.calleeSaved[callee_idx++], type};
        } else {
            allocStackSlot(vreg, type);
        }
    };

    for (auto& [vreg, m] : meta) {
        if (m.call_uses.empty()) {
            if (m.isVar) {
                allocStackSlot(vreg, m.type);
            } else if (m.last_any_use >= 0) {
                // Used only as Add operand (not directly by any call)
                allocCalleeSavedOrStack(vreg, m.type);
            }
            // else: dead — no allocation needed
            continue;
        }

        // Has call uses — determine whether callee-saved is needed.
        bool needs_callee_saved = false;
        if (m.call_uses.size() > 1) {
            needs_callee_saved = true;
        } else {
            int use_idx = m.call_uses.front().first;
            for (int c_idx : call_indices) {
                if (m.def_idx < c_idx && c_idx < use_idx) {
                    needs_callee_saved = true;
                    break;
                }
            }
        }

        if (needs_callee_saved) {
            allocCalleeSavedOrStack(vreg, m.type);
        } else {
            int arg_pos = m.call_uses.front().second;
            BOOST_ASSERT(arg_pos < (int)phys.intArgs.size());
            result[vreg] = PhysLoc{phys.intArgs[arg_pos], m.type};
        }
    }

    // Align frame size to 16 bytes.
    // After pushq %rbp the stack is offset by 8, so we need frameSize ≡ 8 (mod 16)
    // to keep rsp 16-byte aligned before each call.
    int frameSize = alignUp(stack_bytes, 8);
    if (frameSize > 0 && frameSize % 16 == 0) frameSize += 8;

    return {result, frameSize};
}
