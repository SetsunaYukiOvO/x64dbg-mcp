#include "business/SymbolResolver.h"
#include "business/DebugController.h"
#include "core/Logger.h"
#include "core/X64DBGBridge.h"
#include <cstdlib>
#include <cstring>
namespace { int frees = 0; bool owned = true; bool success = true; }
extern "C" {
void BridgeFree(void* data) { ++frees; std::free(data); }
bool DbgGetSymbolInfoAt(duint address, SYMBOLINFO* info) {
    info->addr = address;
    info->decoratedSymbol = owned ? _strdup("decorated") : "borrowed";
    info->undecoratedSymbol = owned ? _strdup("undecorated") : "borrowed";
    info->freeDecorated = info->freeUndecorated = owned;
    return success;
}
}
namespace MCP {
SymbolResolver& SymbolResolver::Instance() { static SymbolResolver instance; return instance; }
std::optional<uint64_t> SymbolResolver::GetAddressFromSymbol(const std::string&) { return 0x1000; }
std::optional<ModuleInfo> SymbolResolver::GetModuleFromAddress(uint64_t) { return std::nullopt; }
DebugController& DebugController::Instance() { static DebugController instance; return instance; }
bool DebugController::IsDebugging() const { return true; }
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}
int main() {
    auto& symbols = MCP::SymbolResolver::Instance();
    if (symbols.GetSymbolFromAddress(0x1000) != "undecorated" || frees != 2) return 1;
    auto info = symbols.GetSymbolInfoFromAddress(0x1000);
    if (!info || info->name != "undecorated" || frees != 4) return 2;
    success = false;
    if (symbols.GetSymbolInfoFromAddress(0x1000) || frees != 6) return 3;
    owned = false; success = true;
    if (symbols.GetSymbolFromAddress(0x1000) != "borrowed" || frees != 6) return 4;
    return 0;
}
