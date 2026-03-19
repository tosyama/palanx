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
        bool     isRetValue   = false;   // true if used by RetPln
        int      retIdx       = 0;       // position in RetPln rets[]
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
            if (c->dst != -1) {
                meta[c->dst].def_idx = i;
                meta[c->dst].type    = c->retType;
            }
        } else if (auto* c = get_if<CallPln>(&instr)) {
            call_indices.push_back(i);
            for (int j = 0; j < (int)c->args.size(); j++)
                meta[c->args[j]].call_uses.push_back({i, j});
            for (int k = 0; k < (int)c->dsts.size(); k++) {
                meta[c->dsts[k]].def_idx = i;
                meta[c->dsts[k]].type    = c->retTypes[k];
            }
        } else if (auto* r = get_if<RetPln>(&instr)) {
            for (int k = 0; k < (int)r->rets.size(); k++) {
                meta[r->rets[k]].last_any_use =
                    max(meta[r->rets[k]].last_any_use, i);
                meta[r->rets[k]].isRetValue = true;
                meta[r->rets[k]].retIdx     = k;
            }
        }
    }

    // Step 2: Assign physical registers or stack slots.
    RegMap result;

    // Pre-bind parameters to intArgs registers.
    for (int j = 0; j < (int)func.params.size(); j++) {
        auto [vreg, type] = func.params[j];
        BOOST_ASSERT(j < (int)phys.intArgs.size());
        result[vreg] = PhysLoc{phys.intArgs[j], type};
    }

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
        if (result.count(vreg)) continue;  // already bound (e.g., param pre-binding)

        if (m.call_uses.empty()) {
            if (m.isVar) {
                allocStackSlot(vreg, m.type);
            } else if (m.isRetValue) {
                // Return value: bind directly to %rax to avoid callee-saved pollution.
                result[vreg] = PhysLoc{"%rax", m.type};
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

    // For non-entry functions, reserve stack slots at the top of the frame to
    // save any callee-saved registers that were allocated.  Shift existing stack
    // slot offsets down to make room, then include the save area in stack_bytes.
    int k = callee_idx;
    if (!func.isEntry && k > 0) {
        int save_area = k * 8;
        for (auto& [vreg, loc] : result) {
            if (loc.isStack())
                loc.stackOffset -= save_area;
        }
        stack_bytes += save_area;
    }

    // Align frame size for ABI compliance.
    // _start (isEntry): entered with RSP 16-byte aligned; pushq %rbp shifts by 8,
    //   so frameSize must be ≡ 8 (mod 16) when non-zero.
    // Regular Palan function: call pushes 8-byte return address, then pushq %rbp
    //   restores 16-byte alignment, so frameSize must be a multiple of 16.
    int frameSize = alignUp(stack_bytes, 8);
    if (func.isEntry) {
        if (frameSize > 0 && frameSize % 16 == 0) frameSize += 8;
    } else {
        frameSize = alignUp(frameSize, 16);
    }

    vector<string> usedCallee;
    for (int i = 0; i < k; i++)
        usedCallee.push_back(phys.calleeSaved[i]);

    return {result, frameSize, usedCallee};
}
