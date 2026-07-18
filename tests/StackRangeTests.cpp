#include "business/StackManager.h"

#include <cstdint>
#include <limits>

int main() {
    using MCP::IsWithinEstimatedStackRange;

    if (!IsWithinEstimatedStackRange(UINT64_C(0), UINT64_C(0x8000)) ||
        !IsWithinEstimatedStackRange(UINT64_C(0x108000), UINT64_C(0x8000)) ||
        IsWithinEstimatedStackRange(UINT64_C(0x108001), UINT64_C(0x8000))) {
        return 1;
    }

    const uint64_t nearMaximum = std::numeric_limits<uint64_t>::max() - UINT64_C(0x8000);
    if (!IsWithinEstimatedStackRange(std::numeric_limits<uint64_t>::max(), nearMaximum) ||
        !IsWithinEstimatedStackRange(nearMaximum - UINT64_C(0x10000), nearMaximum) ||
        IsWithinEstimatedStackRange(nearMaximum - UINT64_C(0x10001), nearMaximum)) {
        return 2;
    }

    const uint64_t maximumX86Address = UINT64_C(0xFFFFFFFF);
    const uint64_t nearX86Maximum = maximumX86Address - UINT64_C(0x8000);
    if (!IsWithinEstimatedStackRange(maximumX86Address, nearX86Maximum, maximumX86Address) ||
        IsWithinEstimatedStackRange(UINT64_C(0x100000000), nearX86Maximum, maximumX86Address)) {
        return 3;
    }

    return 0;
}
