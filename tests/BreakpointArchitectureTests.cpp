#include "business/BreakpointManager.h"

#include <cstdint>

int main() {
    using MCP::HardwareBreakpointCondition;
    using MCP::HardwareBreakpointSize;
    using MCP::IsHardwareBreakpointRequestValid;
    using MCP::IsHardwareBreakpointSizeSupported;

    if (!IsHardwareBreakpointRequestValid(
            UINT64_C(0x1000),
            HardwareBreakpointCondition::Execute,
            HardwareBreakpointSize::Byte1) ||
        IsHardwareBreakpointRequestValid(
            UINT64_C(0x1000),
            HardwareBreakpointCondition::Execute,
            HardwareBreakpointSize::Byte2) ||
        !IsHardwareBreakpointRequestValid(
            UINT64_C(0x1004),
            HardwareBreakpointCondition::Write,
            HardwareBreakpointSize::Byte4) ||
        IsHardwareBreakpointRequestValid(
            UINT64_C(0x1002),
            HardwareBreakpointCondition::ReadWrite,
            HardwareBreakpointSize::Byte4)) {
        return 1;
    }

#ifdef XDBG_ARCH_X64
    if (!IsHardwareBreakpointSizeSupported(HardwareBreakpointSize::Byte8) ||
        !IsHardwareBreakpointRequestValid(
            UINT64_C(0x1008),
            HardwareBreakpointCondition::Write,
            HardwareBreakpointSize::Byte8)) {
        return 2;
    }
#else
    if (IsHardwareBreakpointSizeSupported(HardwareBreakpointSize::Byte8) ||
        IsHardwareBreakpointRequestValid(
            UINT64_C(0x1008),
            HardwareBreakpointCondition::Write,
            HardwareBreakpointSize::Byte8)) {
        return 2;
    }
#endif

    return 0;
}
