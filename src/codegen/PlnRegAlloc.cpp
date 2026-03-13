#include "PlnRegAlloc.h"
#include <boost/assert.hpp>

RegMap allocateRegisters(const VFunc& func, const PhysRegs& phys)
{
    RegMap result;
    int intArgIdx   = 0;
    int floatArgIdx = 0;

    for (auto& instr : func.instrs) {
        if (auto* i = std::get_if<LeaLabel>(&instr)) {
            // Ptr64 -> integer argument register
            BOOST_ASSERT(intArgIdx < (int)phys.intArgs.size());
            result[i->dst] = PhysLoc{phys.intArgs[intArgIdx++], i->type};
        } else if (std::get_if<CallC>(&instr)) {
            // Reset both counters for the next call's arguments
            intArgIdx   = 0;
            floatArgIdx = 0;
        }
        // ExitCode: no registers involved
    }

    return result;
}
