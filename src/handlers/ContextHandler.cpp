#include "ContextHandler.h"
#include "../core/Logger.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include <chrono>

#ifdef XDBG_SDK_AVAILABLE
#include "_plugins.h"
#include "bridgemain.h"
#endif

namespace {
json CaptureContext(const std::string& method, const json& params = json::object()) {
    MCP::JSONRPCRequest request{"2.0", int64_t(1), method, params};
    const auto response = MCP::MethodDispatcher::Instance().Dispatch(request);
    if (response.error) throw MCP::MCPException(response.error->message, response.error->code);
    return response.result;
}
}

json ContextHandler::getSnapshot(const json& params) {
    try {
        // Check if debugger is active
        if (!DbgIsDebugging()) {
            return {
                {"success", false},
                {"error", "Debugger is not active"}
            };
        }

        bool includeStack = true;
        bool includeModules = true;
        bool includeBreakpoints = true;
        bool includeThreads = true;

        // Parse optional parameters
        if (params.contains("include_stack") && params["include_stack"].is_boolean()) {
            includeStack = params["include_stack"];
        }
        if (params.contains("include_modules") && params["include_modules"].is_boolean()) {
            includeModules = params["include_modules"];
        }
        if (params.contains("include_breakpoints") && params["include_breakpoints"].is_boolean()) {
            includeBreakpoints = params["include_breakpoints"];
        }
        if (params.contains("include_threads") && params["include_threads"].is_boolean()) {
            includeThreads = params["include_threads"];
        }

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        json snapshot = {
            {"success", true},
            {"timestamp", timestamp},
            {"registers", captureRegisters()},
            {"state", {
                {"is_debugging", DbgIsDebugging()},
                {"is_running", DbgIsDebugging() && DbgIsRunning()}
            }}
        };

        if (includeStack) {
            snapshot["stack"] = captureStack();
        }

        if (includeModules) {
            snapshot["modules"] = captureModules();
        }

        if (includeBreakpoints) {
            snapshot["breakpoints"] = captureBreakpoints();
        }

        if (includeThreads) {
            snapshot["threads"] = captureThreads();
        }

        return snapshot;

    } catch (const std::exception& e) {
        MCP::Logger::Error("Snapshot capture error: {}", e.what());
        return {
            {"success", false},
            {"error", e.what()}
        };
    }
}

json ContextHandler::getBasicContext(const json& params) {
    try {
        if (!DbgIsDebugging()) {
            return {
                {"success", false},
                {"error", "Debugger is not active"}
            };
        }

        return {
            {"success", true},
            {"registers", captureRegisters()},
            {"state", {
                {"is_debugging", DbgIsDebugging()},
                {"is_running", DbgIsDebugging() && DbgIsRunning()}
            }}
        };

    } catch (const std::exception& e) {
        return {
            {"success", false},
            {"error", e.what()}
        };
    }
}

json ContextHandler::compareSnapshots(const json& params) {
    try {
        if (!params.contains("snapshot1") || !params.contains("snapshot2")) {
            return {
                {"success", false},
                {"error", "Missing snapshot1 or snapshot2 parameters"}
            };
        }

        json snap1 = params["snapshot1"];
        json snap2 = params["snapshot2"];
        json differences = json::object();

        // Compare registers
        if (snap1.contains("registers") && snap2.contains("registers")) {
            json regDiff = json::array();
            for (auto& [key, val1] : snap1["registers"].items()) {
                if (snap2["registers"].contains(key)) {
                    auto val2 = snap2["registers"][key];
                    if (val1 != val2) {
                        regDiff.push_back({
                            {"register", key},
                            {"old_value", val1},
                            {"new_value", val2}
                        });
                    }
                }
            }
            if (!regDiff.empty()) {
                differences["registers"] = regDiff;
            }
        }

        // Compare state
        if (snap1.contains("state") && snap2.contains("state")) {
            if (snap1["state"] != snap2["state"]) {
                differences["state"] = {
                    {"old", snap1["state"]},
                    {"new", snap2["state"]}
                };
            }
        }

        return {
            {"success", true},
            {"has_differences", !differences.empty()},
            {"differences", differences}
        };

    } catch (const std::exception& e) {
        return {
            {"success", false},
            {"error", e.what()}
        };
    }
}

json ContextHandler::captureRegisters() {
    return CaptureContext("register.list");
}

json ContextHandler::captureBreakpoints() {
    return CaptureContext("breakpoint.list").at("breakpoints");
}

json ContextHandler::captureThreads() {
    return CaptureContext("thread.list").at("threads");
}

json ContextHandler::captureStack() {
    return CaptureContext("stack.get_trace", {{"max_depth", 64}}).at("frames");
}

json ContextHandler::captureModules() {
    return CaptureContext("module.list").at("modules");
}
