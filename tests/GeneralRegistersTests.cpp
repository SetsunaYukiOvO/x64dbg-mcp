#include "business/RegisterManager.h"
#include "business/DebugController.h"
#include "core/Logger.h"
#include "core/X64DBGBridge.h"
#include <algorithm>
extern "C" {
duint DbgValFromString(const char*) { return 0; }
bool DbgValToString(const char*, duint) { return true; }
bool DbgGetRegDumpEx(REGDUMP_AVX512*, size_t) { return true; }
}
namespace MCP {
DebugController& DebugController::Instance() { static DebugController instance; return instance; }
bool DebugController::IsPaused() const { return true; }
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}
int main() {
    const auto regs = MCP::RegisterManager::Instance().GetGeneralRegisters();
    const auto has = [&](const char* name) { return std::any_of(regs.begin(), regs.end(), [&](const auto& reg) { return reg.name == name; }); };
#ifdef XDBG_ARCH_X86
    if (regs.size() != 8 || !has("esi") || !has("esp")) return 1;
#else
    if (regs.size() != 16 || !has("rsi") || !has("rsp")) return 2;
#endif
    if (has("cs") || has("es") || has("eip") || has("rip") || has("eflags") || has("rflags")) return 3;
    return 0;
}
