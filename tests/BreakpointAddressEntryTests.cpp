#include <nlohmann/json.hpp>

#include "handlers/BreakpointHandler.h"

#include "business/BreakpointManager.h"
#include "core/Exceptions.h"
#include "core/MethodDispatcher.h"
#include "core/PermissionChecker.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

bool g_breakpointManagerAccessed = false;
std::unordered_map<std::string, MCP::MethodHandler> g_handlers;

const MCP::MethodHandler& GetBreakpointHandler(const std::string& method = "breakpoint.get") {
    return g_handlers.at(method);
}

bool RejectsBeforeBreakpointLookup(const nlohmann::json& address) {
    g_breakpointManagerAccessed = false;
    try {
        (void)GetBreakpointHandler()({{"address", address}});
    } catch (const MCP::InvalidAddressException& ex) {
        return !g_breakpointManagerAccessed &&
               std::string(ex.what()).find("exceeds target architecture range") !=
                   std::string::npos;
    } catch (...) {
        return false;
    }
    return false;
}

bool AcceptsNumericAddressForBreakpointLookup(const nlohmann::json& address) {
    g_breakpointManagerAccessed = false;
    try {
        (void)GetBreakpointHandler()({{"address", address}});
    } catch (const MCP::InvalidAddressException& ex) {
        return g_breakpointManagerAccessed &&
               std::string(ex.what()).find("No breakpoint at address") != std::string::npos;
    } catch (...) {
        return false;
    }
    return false;
}

bool RejectsUnknownBreakpointType(const std::string& method) {
    try {
        if (method == "breakpoint.delete") {
            (void)GetBreakpointHandler(method)({{"address", "0x1000"}, {"type", "unknown"}});
        } else {
            (void)GetBreakpointHandler(method)({{"type", "unknown"}});
        }
    } catch (const MCP::InvalidParamsException&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

} // namespace

namespace MCP {

MethodDispatcher& MethodDispatcher::Instance() {
    static MethodDispatcher instance;
    return instance;
}

void MethodDispatcher::RegisterMethod(const std::string& method, MethodHandler handler) {
    g_handlers[method] = std::move(handler);
}

PermissionChecker& PermissionChecker::Instance() {
    static PermissionChecker instance;
    return instance;
}

bool PermissionChecker::IsBreakpointModificationAllowed() const {
    return true;
}

bool PermissionChecker::IsScriptExecutionAllowed() const {
    return true;
}

BreakpointManager& BreakpointManager::Instance() {
    static BreakpointManager instance;
    return instance;
}

bool BreakpointManager::SetSoftwareBreakpoint(uint64_t, const std::string&) {
    return false;
}

bool BreakpointManager::SetHardwareBreakpoint(
    uint64_t,
    HardwareBreakpointCondition,
    HardwareBreakpointSize,
    const std::string&)
{
    return false;
}

bool BreakpointManager::SetMemoryBreakpoint(uint64_t, size_t, const std::string&) {
    return false;
}

bool BreakpointManager::DeleteBreakpoint(uint64_t, std::optional<BreakpointType>) {
    return false;
}

bool BreakpointManager::EnableBreakpoint(uint64_t) {
    return false;
}

bool BreakpointManager::DisableBreakpoint(uint64_t) {
    return false;
}

bool BreakpointManager::ToggleBreakpoint(uint64_t) {
    return false;
}

std::optional<BreakpointInfo> BreakpointManager::GetBreakpoint(uint64_t) {
    g_breakpointManagerAccessed = true;
    return std::nullopt;
}

std::vector<BreakpointInfo> BreakpointManager::ListBreakpoints(std::optional<BreakpointType>) {
    return {};
}

size_t BreakpointManager::DeleteAllBreakpoints(std::optional<BreakpointType>) {
    return 0;
}

bool BreakpointManager::SetCondition(uint64_t, const std::string&) {
    return false;
}

bool BreakpointManager::SetLogBreakpoint(uint64_t, const std::string&) {
    return false;
}

bool BreakpointManager::ResetHitCount(uint64_t) {
    return false;
}

} // namespace MCP

int main() {
#ifndef XDBG_ARCH_X86
    return 1;
#else
    MCP::BreakpointHandler::RegisterMethods();
    if (g_handlers.find("breakpoint.get") == g_handlers.end()) {
        return 2;
    }

    if (!RejectsBeforeBreakpointLookup("0x100000000") ||
        !RejectsBeforeBreakpointLookup(UINT64_C(0x100000000)) ||
        !AcceptsNumericAddressForBreakpointLookup(UINT64_C(0xFFFFFFFF))) {
        return 3;
    }

    if (!RejectsUnknownBreakpointType("breakpoint.delete") ||
        !RejectsUnknownBreakpointType("breakpoint.list") ||
        !RejectsUnknownBreakpointType("breakpoint.delete_all")) {
        return 4;
    }

    return 0;
#endif
}
