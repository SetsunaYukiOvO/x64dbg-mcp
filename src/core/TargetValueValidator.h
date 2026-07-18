#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(XDBG_ARCH_X64) == defined(XDBG_ARCH_X86)
#error "Define exactly one of XDBG_ARCH_X64 or XDBG_ARCH_X86"
#endif

namespace MCP {

class TargetValueValidator {
public:
    static constexpr uint64_t MaxAddress() noexcept {
#ifdef XDBG_ARCH_X64
        return std::numeric_limits<uint64_t>::max();
#else
        return std::numeric_limits<uint32_t>::max();
#endif
    }

    static constexpr bool FitsAddress(uint64_t address) noexcept {
        return address <= MaxAddress();
    }

    static constexpr bool FitsAddressRange(uint64_t address, uint64_t size) noexcept {
        return size > 0 && FitsAddress(address) && size - 1 <= MaxAddress() - address;
    }

    static constexpr bool FitsUnsignedValue(uint64_t value, size_t sizeBytes) noexcept {
        if (sizeBytes == 0) {
            return false;
        }
        if (sizeBytes >= sizeof(uint64_t)) {
            return true;
        }
        return value <= ((UINT64_C(1) << (sizeBytes * 8)) - 1);
    }
};

} // namespace MCP
