#include "handlers/ContextHandler.h"
#include "core/MethodDispatcher.h"
#include "core/Logger.h"
#include "core/X64DBGBridge.h"
#include <set>
namespace { std::set<std::string> calls; bool fail = false; }
extern "C" {
bool DbgIsDebugging() { return true; }
bool DbgIsRunning() { return false; }
}
namespace MCP {
MethodDispatcher& MethodDispatcher::Instance() { static MethodDispatcher instance; return instance; }
JSONRPCResponse MethodDispatcher::Dispatch(const JSONRPCRequest& request) {
    calls.insert(request.method);
    JSONRPCResponse response{"2.0", request.id, json::object(), std::nullopt};
    if (fail && request.method == "thread.list") {
        response.error = JSONRPCError(-32004, "Thread capture failed"); return response;
    }
    const auto key = request.method == "register.list" ? "registers" : request.method == "thread.list" ? "threads" :
                     request.method == "module.list" ? "modules" : request.method == "breakpoint.list" ? "breakpoints" : "frames";
    response.result[key] = json::array({{{"captured", true}}});
    return response;
}
void Logger::Log(LogLevel, const std::string&) {}
std::string Logger::FormatLogMessage(const std::string& text) { return text; }
}
int main() {
    auto snapshot = ContextHandler::getSnapshot(json::object());
    if (snapshot["success"] != true || calls.size() != 5) return 1;
    for (const auto* field : {"modules", "threads", "stack", "breakpoints"})
        if (snapshot[field].size() != 1 || snapshot[field][0]["captured"] != true) return 2;
    calls.clear();
    snapshot = ContextHandler::getSnapshot({{"include_modules", false}, {"include_stack", false}});
    if (snapshot.contains("modules") || calls.count("module.list") || calls.count("stack.get_trace")) return 3;
    fail = true;
    snapshot = ContextHandler::getSnapshot(json::object());
    if (snapshot["success"] != false || snapshot["error"] != "Thread capture failed") return 4;
    return 0;
}
