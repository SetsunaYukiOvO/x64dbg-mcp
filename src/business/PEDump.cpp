#include "DumpManager.h"
#include "../core/Logger.h"
#include "../utils/PEValidation.h"
#include "../utils/StringUtils.h"
#include <algorithm>

namespace MCP {

bool DumpManager::RebuildPEHeaders(uint64_t moduleBase, std::vector<uint8_t>& buffer,
                                   std::optional<uint32_t> newEP) {
    try {
        const auto layout = ValidatePELayout(buffer);
        if (!layout) return false;
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + layout->ntOffset);

        // 淇鍏ュ彛鐐?
        if (newEP.has_value()) {
            if (*newEP >= buffer.size() || *newEP >= ntHeaders->OptionalHeader.SizeOfImage) return false;
            ntHeaders->OptionalHeader.AddressOfEntryPoint = newEP.value();
            Logger::Info("Updated entry point to RVA: {}",
                         StringUtils::FormatAddress(newEP.value()));
        }
        
        // This buffer is a loaded memory image: relocations in its bytes already
        // reflect moduleBase. Make that base the preferred load address so a loader
        // applies only the delta from this snapshot if it relocates the dump again.
#ifdef XDBG_ARCH_X64
        ntHeaders->OptionalHeader.ImageBase = moduleBase;
#else
        if (moduleBase > UINT32_MAX) return false;
        ntHeaders->OptionalHeader.ImageBase = static_cast<DWORD>(moduleBase);
#endif
        if (!AlignPESections(buffer)) return false;

        Logger::Info("PE headers rebuilt successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to rebuild PE headers: {}", e.what());
        return false;
    }
}


bool DumpManager::ValidatePEHeader(const std::vector<uint8_t>& buffer) {
    return ValidatePELayout(buffer).has_value();
}


bool DumpManager::FixPEChecksum(std::vector<uint8_t>& buffer) {
    try {
        const auto layout = ValidatePELayout(buffer);
        if (!layout) return false;
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + layout->ntOffset);

        // 绠€鍗曞湴娓呴浂鏍￠獙鍜?
        ntHeaders->OptionalHeader.CheckSum = 0;
        
        Logger::Debug("PE checksum cleared");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to fix PE checksum: {}", e.what());
        return false;
    }
}


bool DumpManager::AlignPESections(std::vector<uint8_t>& buffer) {
    try {
        if (!ValidatePEHeader(buffer)) {
            return false;
        }
        
        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer.data());
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + dosHeader->e_lfanew);
        
        uint32_t fileAlignment = ntHeaders->OptionalHeader.FileAlignment;
        if (fileAlignment == 0) {
            fileAlignment = 0x200;
        }
        
        auto* sections = reinterpret_cast<IMAGE_SECTION_HEADER*>(
            reinterpret_cast<uint8_t*>(ntHeaders) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + ntHeaders->FileHeader.SizeOfOptionalHeader
        );

        const auto alignUp = [](uint32_t value, uint32_t alignment) -> uint32_t {
            if (alignment == 0) {
                return value;
            }
            const uint64_t aligned = ((uint64_t(value) + alignment - 1) / alignment) * alignment;
            return static_cast<uint32_t>(std::min<uint64_t>(aligned, UINT32_MAX));
        };
        
        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
            const uint32_t virtualAddress = sections[i].VirtualAddress;
            uint32_t virtualSize = sections[i].Misc.VirtualSize;
            if (virtualSize == 0) {
                virtualSize = sections[i].SizeOfRawData;
            }

            // Dump buffer is in memory-image layout, so raw data must point to RVA.
            sections[i].PointerToRawData = virtualAddress;
            sections[i].SizeOfRawData = alignUp(virtualSize, fileAlignment);

            if (virtualAddress >= buffer.size()) {
                sections[i].PointerToRawData = 0;
                sections[i].SizeOfRawData = 0;
                continue;
            }

            const size_t maxAvailable = buffer.size() - static_cast<size_t>(virtualAddress);
            if (sections[i].SizeOfRawData > maxAvailable) {
                sections[i].SizeOfRawData = static_cast<uint32_t>(maxAvailable);
            }
        }
        
        Logger::Debug("PE sections aligned");
        return true;
        
    } catch (const std::exception& e) {
        Logger::Error("Failed to align PE sections: {}", e.what());
        return false;
    }
}


} // namespace MCP
