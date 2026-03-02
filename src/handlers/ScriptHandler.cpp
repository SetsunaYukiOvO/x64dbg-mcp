#include "ScriptHandler.h"
#include "../core/Logger.h"
#include "../core/PermissionChecker.h"
#include <sstream>

#ifdef XDBG_SDK_AVAILABLE
#include "_plugins.h"
#include "bridgemain.h"
#endif

std::string ScriptHandler::lastResult = "";
bool ScriptHandler::lastSuccess = false;
std::mutex ScriptHandler::resultMutex;

json ScriptHandler::execute(const json& params) {
    try {
        if (!MCP::PermissionChecker::Instance().IsScriptExecutionAllowed()) {
            return {
                {"success", false},
                {"error", "Script execution is disabled by permissions"}
            };
        }

        if (!params.contains("command") || !params["command"].is_string()) {
            return {
                {"success", false},
                {"error", "Missing or invalid 'command' parameter"}
            };
        }

        std::string command = params["command"];
        MCP::Logger::Debug("Executing script command: {}", command);

        bool success = DbgCmdExec(command.c_str());

        {
            std::lock_guard<std::mutex> lock(resultMutex);
            lastSuccess = success;
            lastResult = success ? "Command executed successfully" : "Command execution failed";
        }

        json result = {
            {"success", success},
            {"command", command}
        };

        if (!success) {
            result["error"] = "Command execution failed";
        }

        if (params.contains("capture_output") && params["capture_output"].is_boolean() && params["capture_output"]) {
            std::lock_guard<std::mutex> lock(resultMutex);
            result["output"] = lastResult;
        }

        return result;

    } catch (const std::exception& e) {
        MCP::Logger::Error("Script execution error: {}", e.what());
        return {
            {"success", false},
            {"error", e.what()}
        };
    }
}

json ScriptHandler::executeBatch(const json& params) {
    try {
        if (!MCP::PermissionChecker::Instance().IsScriptExecutionAllowed()) {
            return {
                {"success", false},
                {"error", "Script execution is disabled by permissions"}
            };
        }

        if (!params.contains("commands") || !params["commands"].is_array()) {
            return {
                {"success", false},
                {"error", "Missing or invalid 'commands' parameter (must be array)"}
            };
        }

        json::array_t commands = params["commands"];
        json results = json::array();
        bool allSuccess = true;
        int successCount = 0;
        int failCount = 0;

        bool stopOnError = false;
        if (params.contains("stop_on_error") && params["stop_on_error"].is_boolean()) {
            stopOnError = params["stop_on_error"];
        }

        for (const auto& cmd : commands) {
            if (!cmd.is_string()) {
                results.push_back({
                    {"success", false},
                    {"command", ""},
                    {"error", "Invalid command (not a string)"}
                });
                allSuccess = false;
                failCount++;
                if (stopOnError) {
                    break;
                }
                continue;
            }

            std::string command = cmd;
            bool success = DbgCmdExec(command.c_str());

            json cmdResult = {
                {"success", success},
                {"command", command}
            };

            if (!success) {
                cmdResult["error"] = "Command execution failed";
                allSuccess = false;
                failCount++;
                if (stopOnError) {
                    results.push_back(cmdResult);
                    break;
                }
            } else {
                successCount++;
            }

            results.push_back(cmdResult);
        }

        {
            std::lock_guard<std::mutex> lock(resultMutex);
            lastSuccess = allSuccess;
            if (allSuccess) {
                lastResult = "All " + std::to_string(successCount) + " commands executed successfully";
            } else {
                lastResult = std::to_string(successCount) + " succeeded, " + std::to_string(failCount) + " failed";
            }
        }

        return {
            {"success", allSuccess},
            {"total", commands.size()},
            {"succeeded", successCount},
            {"failed", failCount},
            {"results", results}
        };

    } catch (const std::exception& e) {
        MCP::Logger::Error("Batch script execution error: {}", e.what());
        return {
            {"success", false},
            {"error", e.what()}
        };
    }
}

json ScriptHandler::getLastResult(const json& params) {
    (void)params;

    bool successSnapshot = false;
    std::string resultSnapshot;
    {
        std::lock_guard<std::mutex> lock(resultMutex);
        successSnapshot = lastSuccess;
        resultSnapshot = lastResult;
    }

    json result = {
        {"success", successSnapshot},
        {"result", resultSnapshot}
    };

    if (!successSnapshot) {
        result["error"] = resultSnapshot.empty() ? "No script executed yet or last execution failed" : resultSnapshot;
    }

    return result;
}