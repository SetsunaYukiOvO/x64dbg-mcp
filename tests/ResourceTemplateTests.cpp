#include "core/MCPResourceRegistry.h"
#include "core/MethodDispatcher.h"
#include "core/Exceptions.h"
#include "core/Logger.h"
namespace { std::string method; json parameters; bool fail = false; }
namespace MCP {
MethodDispatcher& MethodDispatcher::Instance() { static MethodDispatcher instance; return instance; }
JSONRPCResponse MethodDispatcher::Dispatch(const JSONRPCRequest& request) {
    method = request.method; parameters = request.params;
    JSONRPCResponse response{"2.0", request.id, {{"captured", true}}, std::nullopt};
    if (fail) response.error = JSONRPCError(-32003, "Denied");
    return response;
}
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}
int main() {
    auto& registry = MCP::MCPResourceRegistry::Instance();
    registry.RegisterDefaultResources();
    const std::pair<const char*, const char*> cases[] = {
        {"memory://0x1000/16", "memory.read"}, {"disassembly://0x1000/4", "disassembly.at"},
        {"module://kernel32.dll/info", "module.get"}, {"module://kernel32.dll/exports", "module.get_exports"},
        {"module://kernel32.dll/imports", "module.get_imports"}, {"symbol://module%21name/resolve", "symbol.resolve"},
        {"function://0x1000/disassembly", "disassembly.function"}, {"breakpoint://0x1000/info", "breakpoint.get"}
    };
    if (registry.GenerateTemplatesListResponse()["resourceTemplates"].size() != 8) return 1;
    for (const auto& item : cases) {
        const auto content = registry.ReadResource(item.first);
        if (method != item.second || json::parse(content.text)["captured"] != true) return 2;
    }
    for (const auto* uri : {"memory://0x1000/4294967297", "disassembly://0x1000/10001", "module://%xx/info", "symbol:///resolve", "unknown://name/info"}) {
        method.clear();
        try { registry.ReadResource(uri); return 3; } catch (const MCP::MCPException&) {}
        if (!method.empty()) return 4;
    }
    fail = true;
    try { registry.ReadResource("memory://0x1000/16"); return 5; }
    catch (const MCP::MCPException& ex) { if (ex.GetCode() != -32003) return 6; }
    return 0;
}
