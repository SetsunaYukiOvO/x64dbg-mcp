#include "business/MemoryManager.h"
#include "business/DebugController.h"
#include "core/Logger.h"
#include "core/X64DBGBridge.h"
#include <cstdlib>
#include <cstring>
namespace {
std::vector<uint8_t> memory(3 * 65536, 0);
bool failMiddle = false;
constexpr duint base = 0x10000;
}
extern "C" {
bool DbgMemRead(duint address, void* out, duint size) {
    if (address < base || size > memory.size() || address - base > memory.size() - size) return false;
    if (failMiddle && address == base + 65536) return false;
    std::memcpy(out, memory.data() + (address - base), size); return true;
}
bool DbgMemWrite(duint, const void*, duint) { return false; }
bool DbgMemIsValidReadPtr(duint) { return true; }
bool DbgGetModuleAt(duint, char*) { return false; }
HANDLE DbgGetProcessHandle() { return nullptr; }
void BridgeFree(void* data) { std::free(data); }
bool DbgMemMap(MEMMAP* map) {
    map->count = 1;
    map->page = static_cast<MEMPAGE*>(std::calloc(1, sizeof(MEMPAGE)));
    map->page->mbi.BaseAddress = reinterpret_cast<void*>(base);
    map->page->mbi.RegionSize = memory.size();
    map->page->mbi.State = MEM_COMMIT;
    map->page->mbi.Protect = PAGE_READWRITE;
    return true;
}
}
namespace MCP {
DebugController& DebugController::Instance() { static DebugController instance; return instance; }
bool DebugController::IsDebugging() const { return true; }
bool DebugController::IsPaused() const { return true; }
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}
int main() {
    memory[10] = memory[65535] = memory[131071] = 0xaa;
    memory[11] = memory[65536] = memory[131072] = 0xbb;
    auto& manager = MCP::MemoryManager::Instance();
    auto found = manager.Search("AA BB");
    if (found.size() != 3 || found[1].address != base + 65535 || found[2].address != base + 131071) return 1;
    if (manager.Search("AA ??", base, base + 65536).size() != 1) return 2;
    if (manager.Search("AA BB", 0, 0, 1).size() != 1) return 3;
    failMiddle = true;
    if (manager.Search("AA BB").size() != 1) return 4;
    return 0;
}
