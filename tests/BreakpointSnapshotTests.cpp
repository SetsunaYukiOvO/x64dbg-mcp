#include "business/BreakpointManager.h"
#include "business/DebugController.h"
#include "core/Logger.h"
#include "core/X64DBGBridge.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
std::vector<BRIDGEBP> entries;
int cachedQueries = 0;
BPXTYPE lastReference = bp_none;
bool Delete(duint address, BPXTYPE type) {
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto& bp) {
        return bp.addr == address && bp.type == type;
    }), entries.end());
    return true;
}
}
extern "C" {
const DBGFUNCTIONS* DbgFunctions() {
    static DBGFUNCTIONS functions{};
    functions.MemBpSize = [](duint) -> duint { return 4096; };
    functions.BpRefVa = [](BP_REF* ref, BPXTYPE type, duint address) {
        lastReference = type; ref->type = type; ref->offset = address; return true;
    };
    functions.BpSetFieldText = [](const BP_REF*, BP_FIELD, const char*) { return true; };
    functions.BpSetFieldNumber = [](const BP_REF*, BP_FIELD, duint) { return true; };
    functions.BpGetFieldNumber = [](const BP_REF*, BP_FIELD, duint*) { return false; };
    return &functions;
}
int DbgGetBpList(BPXTYPE, BPMAP* map) {
    map->count = static_cast<int>(entries.size());
    map->bp = static_cast<BRIDGEBP*>(std::malloc(entries.size() * sizeof(BRIDGEBP)));
    if (!entries.empty()) std::memcpy(map->bp, entries.data(), entries.size() * sizeof(BRIDGEBP));
    return map->count;
}
void BridgeFree(void* data) { std::free(data); }
BPXTYPE DbgGetBpxTypeAt(duint) { ++cachedQueries; return bp_hardware; }
bool DbgCmdExecDirect(const char* command) {
    const std::string text(command);
    const auto space = text.find(' ');
    const auto address = static_cast<duint>(std::stoull(text.substr(space + 1), nullptr, 16));
    const auto verb = text.substr(0, space);
    if (verb == "bpmc") return Delete(address, bp_memory);
    const auto type = verb == "bpe" || verb == "bpd" ? bp_normal :
                      verb == "bphe" || verb == "bphd" ? bp_hardware : bp_memory;
    for (auto& bp : entries) if (bp.addr == address && bp.type == type) {
        bp.enabled = verb == "bpe" || verb == "bphe" || verb == "bpme";
        return true;
    }
    return false;
}
}
namespace Script { namespace Debug {
bool SetBreakpoint(duint) { return true; }
bool DeleteBreakpoint(duint address) { return Delete(address, bp_normal); }
bool DisableBreakpoint(duint) { return true; }
bool DeleteHardwareBreakpoint(duint address) { return Delete(address, bp_hardware); }
} }
namespace MCP {
DebugController& DebugController::Instance() { static DebugController instance; return instance; }
bool DebugController::IsDebugging() const { return true; }
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}

int main() {
    auto& manager = MCP::BreakpointManager::Instance();
    for (auto type : {bp_hardware, bp_memory, bp_normal}) {
        BRIDGEBP bp{}; bp.type = type; bp.addr = 0x1000; bp.enabled = false; bp.hitCount = 7;
        entries = {bp};
        if (!manager.HasBreakpoint(0x1000) || manager.ListBreakpoints().size() != 1) return 1;
        auto result = manager.GetBreakpoint(0x1000);
        if (!result || result->enabled || result->hitCount != 7) return 2;
        if (!manager.EnableBreakpoint(0x1000) || !entries[0].enabled || entries.size() != 1) return 8;
        if (!manager.DisableBreakpoint(0x1000) || entries[0].enabled) return 9;
        if (!manager.SetCondition(0x1000, "1") || lastReference != type) return 3;
        if (!manager.ResetHitCount(0x1000) || lastReference != type) return 4;
        if (manager.DeleteAllBreakpoints() != 1 || !entries.empty()) return 5;
    }
    for (auto type : {bp_memory, bp_hardware, bp_normal}) {
        BRIDGEBP bp{}; bp.type = type; bp.addr = 0x1000; entries.push_back(bp);
    }
    if (manager.GetBreakpoint(0x1000)->type != MCP::BreakpointType::Software) return 6;
    if (!manager.EnableBreakpoint(0x1000, MCP::BreakpointType::Memory) || !entries[0].enabled || entries[1].enabled || entries[2].enabled) return 10;
    if (manager.DeleteAllBreakpoints() != 3 || !entries.empty() || cachedQueries) return 7;
    return 0;
}
