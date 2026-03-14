#include "PlnRegAlloc.h"
#include <boost/assert.hpp>
#include <algorithm>

using namespace std;

RegMap allocateRegisters(const VFunc& func, const PhysRegs& phys)
{
    // Step 1: Collect VReg metadata.
    struct VRegMeta {
        VRegType type   = VRegType::Int64;
        int      def_idx = -1;
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
        } else if (auto* c = get_if<CallC>(&instr)) {
            call_indices.push_back(i);
            for (int j = 0; j < (int)c->args.size(); j++) {
                meta[c->args[j]].uses.push_back({i, j});
            }
        } else if (auto* l = get_if<LoadFromStack>(&instr)) {
            meta[l->dst].def_idx = i;
            meta[l->dst].type    = l->type;
        }
    }

    // Step 2: Assign physical registers.
    RegMap result;
    int callee_idx = 0;

    for (auto& [vreg, m] : meta) {
        if (m.uses.empty()) continue;  // defined but never used as call arg

        int last_use_idx = max_element(m.uses.begin(), m.uses.end())->first;

        // A VReg needs a callee-saved register if:
        //   (a) it is used as an argument in more than one call (must survive across calls), or
        //   (b) there is a call between its definition and its (single) use that would
        //       clobber a caller-saved register.
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
            BOOST_ASSERT(callee_idx < (int)phys.calleeSaved.size());
            result[vreg] = PhysLoc{phys.calleeSaved[callee_idx++], m.type};
        } else {
            // Assign directly to the required argument register.
            int arg_pos = m.uses.front().second;
            BOOST_ASSERT(arg_pos < (int)phys.intArgs.size());
            result[vreg] = PhysLoc{phys.intArgs[arg_pos], m.type};
        }
    }

    return result;
}
