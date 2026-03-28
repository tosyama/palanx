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
        } else if (auto* s = get_if<Sub>(&instr)) {
            meta[s->dst].def_idx = i;
            meta[s->dst].type    = s->type;
            meta[s->lhs].last_any_use = max(meta[s->lhs].last_any_use, i);
            meta[s->rhs].last_any_use = max(meta[s->rhs].last_any_use, i);
        } else if (auto* m = get_if<Mul>(&instr)) {
            meta[m->dst].def_idx = i;
            meta[m->dst].type    = m->type;
            meta[m->lhs].last_any_use = max(meta[m->lhs].last_any_use, i);
            meta[m->rhs].last_any_use = max(meta[m->rhs].last_any_use, i);
        } else if (auto* d = get_if<Div>(&instr)) {
            meta[d->dst].def_idx = i;
            meta[d->dst].type    = d->type;
            meta[d->lhs].last_any_use = max(meta[d->lhs].last_any_use, i);
            meta[d->rhs].last_any_use = max(meta[d->rhs].last_any_use, i);
        } else if (auto* md = get_if<Mod>(&instr)) {
            meta[md->dst].def_idx = i;
            meta[md->dst].type    = md->type;
            meta[md->lhs].last_any_use = max(meta[md->lhs].last_any_use, i);
            meta[md->rhs].last_any_use = max(meta[md->rhs].last_any_use, i);
        } else if (auto* n = get_if<Neg>(&instr)) {
            meta[n->dst].def_idx = i;
            meta[n->dst].type    = n->type;
            meta[n->src].last_any_use = max(meta[n->src].last_any_use, i);
        } else if (auto* cm = get_if<Cmp>(&instr)) {
            meta[cm->dst].def_idx = i;
            meta[cm->dst].type    = VRegType::Int32;
            meta[cm->lhs].last_any_use = max(meta[cm->lhs].last_any_use, i);
            meta[cm->rhs].last_any_use = max(meta[cm->rhs].last_any_use, i);
        } else if (auto* c = get_if<CallC>(&instr)) {
            call_indices.push_back(i);
            for (int j = 0; j < (int)c->args.size(); j++) {
                if (j < (int)phys.intArgs.size())
                    meta[c->args[j]].call_uses.push_back({i, j});
                else
                    meta[c->args[j]].last_any_use = max(meta[c->args[j]].last_any_use, i);
            }
            if (c->dst != -1) {
                meta[c->dst].def_idx = i;
                meta[c->dst].type    = c->retType;
            }
        } else if (auto* c = get_if<CallPln>(&instr)) {
            call_indices.push_back(i);
            for (int j = 0; j < (int)c->args.size(); j++) {
                if (j < (int)phys.intArgs.size())
                    meta[c->args[j]].call_uses.push_back({i, j});
                else
                    meta[c->args[j]].last_any_use = max(meta[c->args[j]].last_any_use, i);
            }
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
        } else if (auto* cj = get_if<CondJmp>(&instr)) {
            meta[cj->cond].last_any_use = max(meta[cj->cond].last_any_use, i);
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

    // Pre-bind parameters to intArgs registers.
    // If a parameter's live range crosses a call, move it to a callee-saved
    // register so it survives the call (caller-saved arg regs are clobbered).
    vector<tuple<string, VReg, VRegType>> pendingParamCopies;
    for (int j = 0; j < (int)func.params.size(); j++) {
        auto [vreg, type] = func.params[j];

        if (j >= (int)phys.intArgs.size()) {
            // Stack parameter: passed by caller above %rbp → 16(%rbp), 24(%rbp), ...
            // The caller's stack is stable across all calls within this function.
            int offset = (j - (int)phys.intArgs.size() + 2) * 8;
            result[vreg] = PhysLoc{"", type, offset};
            continue;
        }

        const VRegMeta& m = meta[vreg];
        int last_use = m.last_any_use;
        for (auto& [ci, pos] : m.call_uses)
            last_use = max(last_use, ci);

        bool crosses_call = false;
        for (int c_idx : call_indices) {
            if (c_idx < last_use) {
                crosses_call = true;
                break;
            }
        }

        if (crosses_call) {
            allocCalleeSavedOrStack(vreg, type);
            pendingParamCopies.push_back({phys.intArgs[j], vreg, type});
        } else {
            result[vreg] = PhysLoc{phys.intArgs[j], type};
        }
    }

    // Count how many vregs are return values (to detect multi-return)
    int numRetValues = 0;
    for (auto& [vreg, m] : meta)
        if (m.isRetValue) numRetValues++;

    for (auto& [vreg, m] : meta) {
        if (result.count(vreg)) continue;  // already bound (e.g., param pre-binding)
        if (m.isVar && m.call_uses.empty()) continue;  // Pass B handles stack-only isVar

        if (m.call_uses.empty()) {
            if (m.isRetValue && numRetValues == 1) {
                // Single return value: bind directly to %rax to avoid callee-saved pollution.
                result[vreg] = PhysLoc{"%rax", m.type};
            } else if (m.isRetValue) {
                // Multi-return value: use callee-saved so each survives until RetPln.
                allocCalleeSavedOrStack(vreg, m.type);
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
            // If another vreg mapped to the desired register is still live at this vreg's
            // definition point, we'd clobber it.  Detect this and use callee-saved instead.
            // (e.g. Mod dst and the rhs param both want %rsi, but rhs is still live at Mod.)
            const string& desired = phys.intArgs[arg_pos];
            bool conflict = false;
            for (auto& [vr, loc] : result) {
                if (loc.isStack() || loc.base != desired) continue;
                // Check if vr is live at m.def_idx.
                // Params have def_idx=-1 (never set by an instruction); treat them as
                // defined at 0 (live from function entry).
                const VRegMeta& um = meta[vr];
                int u_def = (um.def_idx < 0) ? 0 : um.def_idx;
                if (u_def > m.def_idx) continue;
                int u_last = um.last_any_use;
                for (auto& [ci, pos] : um.call_uses)
                    u_last = max(u_last, ci);
                // Strict ">": if u_last == m.def_idx the existing VReg's last use is the
                // same instruction as m's definition (e.g. call args die when the call
                // produces its result), so no real conflict.
                if (u_last > m.def_idx) { conflict = true; break; }
            }
            if (conflict) {
                allocCalleeSavedOrStack(vreg, m.type);
            } else {
                result[vreg] = PhysLoc{desired, m.type};
            }
        }
    }

    // Pass B: assign stack slots to InitVar VRegs in instruction order,
    // reusing freed slots via freePool.
    {
        map<int, vector<int>> freePool;  // size(bytes) -> available offsets

        for (auto& instr : func.instrs) {
            if (auto* v = std::get_if<InitVar>(&instr)) {
                if (result.count(v->dst)) continue;  // already handled in Pass A (had call uses)
                int sz     = sizeOfType(v->type);
                auto& pool = freePool[sz];
                if (!pool.empty()) {
                    int off = pool.back();
                    pool.pop_back();
                    result[v->dst] = PhysLoc{"", v->type, off};
                } else {
                    allocStackSlot(v->dst, v->type);
                }
            } else if (auto* bl = std::get_if<BlockLeave>(&instr)) {
                for (VReg vr : bl->expiredVars) {
                    if (result.count(vr) && result[vr].isStack())
                        freePool[sizeOfType(result[vr].type)].push_back(result[vr].stackOffset);
                }
            }
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

    vector<RegAllocResult::ParamCopy> paramCopies;
    for (auto& [argReg, vreg, ptype] : pendingParamCopies)
        paramCopies.push_back({argReg, result[vreg], ptype});

    return {result, frameSize, usedCallee, paramCopies};
}
