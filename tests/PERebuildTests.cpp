#include "business/DumpManager.h"
#include "core/Logger.h"
#include "utils/PEValidation.h"
namespace MCP {
DumpManager& DumpManager::Instance() { static DumpManager manager; return manager; }
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}
int main() {
    std::vector<uint8_t> data(4096);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE; dos->e_lfanew = 128;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(data.data() + 128);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER);
    nt->FileHeader.NumberOfSections = 1;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR_MAGIC;
    nt->OptionalHeader.SizeOfImage = 4096;
#ifdef XDBG_ARCH_X64
    const uint64_t runtimeBase = 0x180000000ULL;
#else
    const uint64_t runtimeBase = 0x600000;
#endif
    nt->OptionalHeader.ImageBase = static_cast<decltype(nt->OptionalHeader.ImageBase)>(runtimeBase);
    auto& manager = MCP::DumpManager::Instance();
    if (!manager.RebuildPEHeaders(runtimeBase, data, 512) || nt->OptionalHeader.ImageBase != runtimeBase) return 1;
    // A second relocation must start from the actual base of the captured bytes.
    if (!manager.RebuildPEHeaders(runtimeBase + 0x10000, data) || nt->OptionalHeader.ImageBase != runtimeBase + 0x10000) return 2;
    const auto valid = data;
    dos->e_lfanew = -1;
    auto malformed = data;
    if (manager.RebuildPEHeaders(runtimeBase, data) || data != malformed) return 3;
    data = valid;
    nt = reinterpret_cast<IMAGE_NT_HEADERS*>(data.data() + 128);
    nt->FileHeader.NumberOfSections = 65535;
    malformed = data;
    if (manager.RebuildPEHeaders(runtimeBase, data) || data != malformed) return 4;
    return 0;
}
