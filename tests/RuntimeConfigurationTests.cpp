#include "core/ConfigManager.h"
#include "core/Logger.h"
#include "core/MethodDispatcher.h"
#include "core/Exceptions.h"
#include "core/X64DBGBridge.h"
#include "communication/MCPHttpServer.h"
#include "handlers/PatchHandler.h"
#include "handlers/EventCallbackHandler.h"
#include <filesystem>
#include <unordered_map>
namespace {
std::unordered_map<std::string, MCP::MethodHandler> handlers;
int sdkAccesses = 0, restores = 0, broadcasts = 0;
}
extern "C" {
const DBGFUNCTIONS* DbgFunctions() {
    ++sdkAccesses;
    static DBGFUNCTIONS functions{};
    functions.PatchRestore = [](duint) { ++restores; return true; };
    return &functions;
}
}
namespace MCP {
MethodDispatcher& MethodDispatcher::Instance() { static MethodDispatcher instance; return instance; }
void MethodDispatcher::RegisterMethod(const std::string& method, MethodHandler handler) { handlers[method] = std::move(handler); }
bool MCPHttpServer::BroadcastNotification(const std::string&, const json&) { ++broadcasts; return true; }
}
int main() {
    auto& config = MCP::ConfigManager::Instance();
    config.Set("timeout.step_timeout_ms", 12);
    if (config.GetStepTimeout() != 12) return 1;
    for (const auto invalid : {-1LL, 0LL, 4294967297LL}) {
        config.Set("timeout.step_timeout_ms", invalid);
        if (config.GetStepTimeout() != 10000) return 2;
    }
    MCP::PatchHandler::RegisterMethods();
    config.Set("permissions.allow_memory_write", false);
    try { handlers.at("patch.restore")({{"address", "0x1000"}}); return 3; }
    catch (const MCP::PermissionDeniedException&) {}
    if (sdkAccesses || restores) return 4;
    config.Set("permissions.allow_memory_write", true);
    if (!handlers.at("patch.restore")({{"address", "0x1000"}})["success"].get<bool>() || restores != 1) return 5;
    config.Set("features.enable_notifications", false);
    MCP::EventCallbackHandler::Instance().Initialize();
    MCP::EventCallbackHandler::OnBreakpoint(0x1000);
    if (broadcasts) return 6;
    config.Set("features.enable_notifications", true);
    MCP::EventCallbackHandler::Instance().Initialize();
    MCP::EventCallbackHandler::OnBreakpoint(0x1000);
    if (broadcasts != 1) return 7;
    const auto path = std::filesystem::temp_directory_path() / ("xdbg-review-log-" + std::to_string(GetCurrentProcessId()));
    if (!MCP::Logger::Initialize(path.u8string(), MCP::LogLevel::Info, false, 1024)) return 8;
    for (int i = 0; i < 30; ++i) MCP::Logger::Info(std::string(100, 'x'));
    MCP::Logger::Shutdown();
    const auto backup = std::filesystem::u8path(path.u8string() + ".1");
    if (std::filesystem::file_size(path) > 1024 || !std::filesystem::exists(backup) || std::filesystem::file_size(backup) > 1024) return 9;
    std::filesystem::remove(path); std::filesystem::remove(backup);
    return 0;
}
