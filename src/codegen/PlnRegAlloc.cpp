#include "PlnRegAlloc.h"
#include <boost/assert.hpp>
#include <algorithm>

using namespace std;

// Visitor helper: overloaded{f1, f2, ...} dispatches std::visit to the matching overload.
// All VInstr types must be handled; omitting one causes a compile error.
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

RegAllocResult allocateRegisters(const VFunc& func, const PhysRegs& phys)
{
    // Step 1: Collect VReg metadata.
    struct VRegMeta {
        VRegType type         = VRegType::Int64;
        int      def_idx      = -1;
        bool     isVar        = false;   // true if defined by InitVar (variable)
        int      last_any_use = -1;      // last use by non-call-argument instructions
        vector<pair<int,int>> call_uses; // (call_instr_idx, arg_position)
        bool     isRetValue   = false;   // true if used by RetPln
    };
    map<VReg, VRegMeta> meta;

    vector<int> call_indices;    // instruction indices of all CallC/CallPln
    vector<int> divmod_indices;  // instruction indices of all Div/Mod (%rax/%rdx clobbered)
    map<string, int> label_idx;  // label name → instruction index (for loop detection)

    for (int i = 0; i < (int)func.instrs.size(); i++) {
        auto& instr = func.instrs[i];

        auto setDef = [&](VReg dst, VRegType type) {
            meta[dst].def_idx = i;
            meta[dst].type    = type;
        };
        auto addUse = [&](VReg vreg) {
            meta[vreg].last_any_use = max(meta[vreg].last_any_use, i);
        };
        auto setBinOp = [&](VReg dst, VReg lhs, VReg rhs, VRegType type) {
            setDef(dst, type);
            addUse(lhs);
            addUse(rhs);
        };
        auto addCallArgs = [&](const vector<VReg>& args) {
            for (int j = 0; j < (int)args.size(); j++) {
                if (j < (int)phys.intArgs.size())
                    meta[args[j]].call_uses.push_back({i, j});
                else
                    addUse(args[j]);
            }
        };

        std::visit(overloaded{
            [&](const LeaLabel& l)  { setDef(l.dst, l.type); },
            [&](const MovImm& m)    { setDef(m.dst, m.type); },
            [&](const InitVar& v)   { setDef(v.dst, v.type); meta[v.dst].isVar = true; },
            [&](const InitVarF& v)  { setDef(v.dst, v.type); meta[v.dst].isVar = true; },
            [&](const Add& a)      { setBinOp(a.dst, a.lhs, a.rhs, a.type); },
            [&](const Sub& s)      { setBinOp(s.dst, s.lhs, s.rhs, s.type); },
            [&](const Mul& m)      { setBinOp(m.dst, m.lhs, m.rhs, m.type); },
            [&](const Div& d)      { divmod_indices.push_back(i); setBinOp(d.dst, d.lhs, d.rhs, d.type); },
            [&](const Mod& m)      { divmod_indices.push_back(i); setBinOp(m.dst, m.lhs, m.rhs, m.type); },
            [&](const Neg& n)      { setDef(n.dst, n.type); addUse(n.src); },
            [&](const Cmp& c)      { setDef(c.dst, VRegType::Int32); addUse(c.lhs); addUse(c.rhs); },
            [&](const Convert& c)  { setDef(c.dst, c.to); addUse(c.src); },
            [&](const CallC& c) {
                call_indices.push_back(i);
                addCallArgs(c.args);
                if (c.dst != -1) setDef(c.dst, c.retType);
            },
            [&](const CallPln& c) {
                call_indices.push_back(i);
                addCallArgs(c.args);
                for (int k = 0; k < (int)c.dsts.size(); k++)
                    setDef(c.dsts[k], c.retTypes[k]);
            },
            [&](const RetPln& r) {
                for (VReg vr : r.rets) {
                    addUse(vr);
                    meta[vr].isRetValue = true;
                }
            },
            [&](const CondJmp& cj) { addUse(cj.cond); },
            [&](const Label& lbl)  { label_idx[lbl.name] = i; },
            // Mov copies src into dst (the variable's canonical VReg).
            // dst is always isVar (allocated by Pass B); only src needs tracking here.
            [&](const Mov& mv)     { addUse(mv.src); },
            [&](const ExitCode&)   {},
            [&](const BlockEnter&) {},
            [&](const BlockLeave&) {},
            [&](const Jmp&)        {},
        }, instr);
    }

    // Collect loop regions: backward jumps define [loop_start_label_idx, jmp_idx].
    // A parameter used inside a loop that also contains a call effectively crosses
    // that call (next iteration re-executes the use after the call).
    vector<pair<int,int>> loop_regions;
    for (int i = 0; i < (int)func.instrs.size(); i++) {
        if (auto* j = get_if<Jmp>(&func.instrs[i])) {
            auto it = label_idx.find(j->label);
            if (it != label_idx.end() && it->second < i)
                loop_regions.push_back({it->second, i});
        }
    }

    // Step 2: Assign physical registers or stack slots.
    RegMap result;

    int callee_idx  = 0;
    int stack_bytes = 0;  // bytes consumed below %rbp so far

    static auto sizeOfType = [](VRegType type) -> int {
        switch (type) {
            case VRegType::Int8:   return 1;
            case VRegType::Int16:  return 2;
            case VRegType::Int32:  return 4;
            case VRegType::Float32: return 4;
            default:               return 8;  // Int64, Ptr64, Float64
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
        // Loop check: if a use and a call both appear in the same loop body,
        // the backward jump causes re-execution of the use after the call.
        if (!crosses_call) {
            for (auto& [ls, le] : loop_regions) {
                bool use_in_loop = (m.last_any_use >= ls && m.last_any_use <= le);
                if (!use_in_loop) {
                    for (auto& [ci, pos] : m.call_uses)
                        if (ci >= ls && ci <= le) { use_in_loop = true; break; }
                }
                if (!use_in_loop) continue;
                for (int c_idx : call_indices) {
                    if (c_idx >= ls && c_idx <= le) {
                        crosses_call = true;
                        break;
                    }
                }
                if (crosses_call) break;
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
        if (m.isVar) continue;  // Pass B always allocates isVar to stack

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
            // %rax and %rdx are clobbered by every Div/Mod instruction (idivq uses
            // %rdx:%rax as dividend and overwrites both).  If this vreg's live range
            // spans any Div/Mod, avoid assigning it to those registers.
            if (!conflict && (desired == "%rdx" || desired == "%rax")) {
                int live_start = m.def_idx < 0 ? 0 : m.def_idx;
                int live_end   = m.call_uses.front().first;
                for (int dm : divmod_indices) {
                    if (dm > live_start && dm < live_end) {
                        conflict = true;
                        break;
                    }
                }
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
            auto allocInitVar = [&](VReg dst, VRegType type) {
                if (result.count(dst)) return;  // already handled in Pass A
                int sz     = sizeOfType(type);
                auto& pool = freePool[sz];
                if (!pool.empty()) {
                    int off = pool.back();
                    pool.pop_back();
                    result[dst] = PhysLoc{"", type, off};
                } else {
                    allocStackSlot(dst, type);
                }
            };
            if (auto* v = std::get_if<InitVar>(&instr)) {
                allocInitVar(v->dst, v->type);
            } else if (auto* v = std::get_if<InitVarF>(&instr)) {
                allocInitVar(v->dst, v->type);
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
            if (loc.isStack() && loc.stackOffset < 0)
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
