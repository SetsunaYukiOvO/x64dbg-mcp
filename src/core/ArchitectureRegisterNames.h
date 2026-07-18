#pragma once

namespace MCP {

struct ArchitectureRegisterNames {
#if defined(XDBG_ARCH_X64) && !defined(XDBG_ARCH_X86)
    inline static constexpr const char* InstructionPointer = "rip";
    inline static constexpr const char* StackPointer = "rsp";
    inline static constexpr const char* BasePointer = "rbp";
#elif defined(XDBG_ARCH_X86) && !defined(XDBG_ARCH_X64)
    inline static constexpr const char* InstructionPointer = "eip";
    inline static constexpr const char* StackPointer = "esp";
    inline static constexpr const char* BasePointer = "ebp";
#else
#error "Define exactly one of XDBG_ARCH_X64 or XDBG_ARCH_X86"
#endif
};

} // namespace MCP
