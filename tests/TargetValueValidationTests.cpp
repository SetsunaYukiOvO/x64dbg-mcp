#include "core/TargetValueValidator.h"

#include <cstdint>

int main() {
#ifdef XDBG_ARCH_X86
    if (!MCP::TargetValueValidator::FitsAddress(UINT64_C(0xFFFFFFFF)) ||
        MCP::TargetValueValidator::FitsAddress(UINT64_C(0x100000000))) {
        return 1;
    }
#else
    if (!MCP::TargetValueValidator::FitsAddress(UINT64_C(0xFFFFFFFF)) ||
        !MCP::TargetValueValidator::FitsAddress(UINT64_C(0x100000000))) {
        return 1;
    }
#endif

    if (!MCP::TargetValueValidator::FitsUnsignedValue(UINT64_C(0xFFFFFFFF), 4) ||
        MCP::TargetValueValidator::FitsUnsignedValue(UINT64_C(0x100000000), 4)) {
        return 2;
    }

    const uint64_t maximumAddress = MCP::TargetValueValidator::MaxAddress();
    if (!MCP::TargetValueValidator::FitsAddressRange(maximumAddress, UINT64_C(1)) ||
        !MCP::TargetValueValidator::FitsAddressRange(maximumAddress - UINT64_C(15), UINT64_C(16)) ||
        MCP::TargetValueValidator::FitsAddressRange(maximumAddress - UINT64_C(15), UINT64_C(17)) ||
        MCP::TargetValueValidator::FitsAddressRange(UINT64_C(0), UINT64_C(0))) {
        return 3;
    }

    return 0;
}
