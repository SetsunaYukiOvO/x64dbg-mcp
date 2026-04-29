#include "DebugHandler.h"
#include "../business/DebugController.h"
#include "../core/MethodDispatcher.h"
#include "../core/RequestValidator.h"
#include "../core/Logger.h"
#include "../utils/StringUtils.h"
#include <windows.h>

namespace MCP {

void DebugHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("debug.get_state", GetState);
    dispatcher.RegisterMethod("debug.run", Run);
    dispatcher.RegisterMethod("debug.pause", Pause);
    dispatcher.RegisterMethod("debug.step_into", StepInto);
    dispatcher.RegisterMethod("debug.step_over", StepOver);
    dispatcher.RegisterMethod("debug.step_out", StepOut);
    dispatcher.RegisterMethod("debug.run_to", RunTo);
    dispatcher.RegisterMethod("debug.restart", Restart);
    dispatcher.RegisterMethod("debug.init", Init);
    dispatcher.RegisterMethod("debug.stop", Stop);
    
    Logger::Info("Registered debug.* methods");
}

json DebugHandler::GetState(const json& params) {
    auto& controller = DebugController::Instance();
    
    DebugState state = controller.GetState();
    std::string stateStr = StateToString(state);
    
    json result = {
        {"state", stateStr}
    };
    
    if (controller.IsDebugging()) {
        try {
            uint64_t rip = controller.GetInstructionPointer();
            result["rip"] = StringUtils::FormatAddress(rip);
        } catch (...) {
            // 如果无法获取 RIP，忽略错误
        }
    }
    
    return result;
}

json DebugHandler::Run(const json& params) {
    auto& controller = DebugController::Instance();
    
    bool success = controller.Run();
    
    json result = {
        {"success", success}
    };
    
    // 等待一小段时间让调试器状态稳定
    Sleep(50);
    
    // 获取当前状态
    DebugState state = controller.GetState();
    result["state"] = StateToString(state);
    
    // 如果暂停了，返回停止原因
    if (controller.IsPaused()) {
        try {
            uint64_t rip = controller.GetInstructionPointer();
            result["rip"] = StringUtils::FormatAddress(rip);
            result["stop_reason"] = "breakpoint_or_exception";
        } catch (...) {
            // 忽略
        }
    }
    
    return result;
}

json DebugHandler::Pause(const json& params) {
    auto& controller = DebugController::Instance();
    
    bool success = controller.Pause();
    uint64_t rip = 0;
    
    if (success && controller.IsPaused()) {
        try {
            rip = controller.GetInstructionPointer();
        } catch (...) {
            // 忽略
        }
    }
    
    json result = {
        {"success", success}
    };
    
    if (rip != 0) {
        result["rip"] = StringUtils::FormatAddress(rip);
    }
    
    return result;
}

json DebugHandler::StepInto(const json& params) {
    auto& controller = DebugController::Instance();
    
    uint64_t rip = controller.StepInto();
    
    return {
        {"rip", StringUtils::FormatAddress(rip)}
    };
}

json DebugHandler::StepOver(const json& params) {
    auto& controller = DebugController::Instance();
    
    uint64_t rip = controller.StepOver();
    
    return {
        {"rip", StringUtils::FormatAddress(rip)}
    };
}

json DebugHandler::StepOut(const json& params) {
    auto& controller = DebugController::Instance();
    
    uint64_t rip = controller.StepOut();
    
    return {
        {"rip", StringUtils::FormatAddress(rip)}
    };
}

json DebugHandler::RunTo(const json& params) {
    RequestValidator::RequireString(params, "address");
    
    std::string addressStr = params["address"].get<std::string>();
    uint64_t address = RequestValidator::ValidateAddress(addressStr);
    
    auto& controller = DebugController::Instance();
    bool success = controller.RunToAddress(address);
    
    return {
        {"success", success},
        {"address", StringUtils::FormatAddress(address)}
    };
}

json DebugHandler::Restart(const json& params) {
    auto& controller = DebugController::Instance();
    
    bool success = controller.Restart();
    
    return {
        {"success", success}
    };
}

json DebugHandler::Init(const json& params) {
    auto& controller = DebugController::Instance();

    std::string path = RequestValidator::GetString(params, "path", "");
    std::string arguments = RequestValidator::GetString(params, "arguments", "");
    std::string currentDir = RequestValidator::GetString(params, "current_dir", "");

    if (path.empty()) {
        // Fall back to the cached path from a previous session.
        path = controller.GetLastDebuggedPath();
    }

    if (path.empty()) {
        return {
            {"success", false},
            {"error", "No executable path provided and no previously debugged target cached"}
        };
    }

    bool success = controller.Init(path, arguments, currentDir);

    json result = {
        {"success", success},
        {"path", path}
    };

    if (!arguments.empty()) {
        result["arguments"] = arguments;
    }
    if (!currentDir.empty()) {
        result["current_dir"] = currentDir;
    }

    return result;
}

json DebugHandler::Stop(const json& params) {
    auto& controller = DebugController::Instance();
    
    bool success = controller.Stop();
    
    return {
        {"success", success}
    };
}

std::string DebugHandler::StateToString(DebugState state) {
    switch (state) {
        case DebugState::Stopped:
            return "stopped";
        case DebugState::Running:
            return "running";
        case DebugState::Paused:
            return "paused";
        default:
            return "unknown";
    }
}

} // namespace MCP
